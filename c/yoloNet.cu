/*
 * Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "yoloNet.h"
#include "cudaUtility.h"
#include "cudaResize.h"
#include "cudaColorspace.h"
#include <cmath>



template<typename T>
__global__ void gpuDetectionOverlay( T* input, T* output, int width, int height, yoloNet::Detection* detections, int numDetections, float4* colors ) 
{
	const int x = blockIdx.x * blockDim.x + threadIdx.x;
	const int y = blockIdx.y * blockDim.y + threadIdx.y;

	if( x >= width || y >= height )
		return;

	const int px_idx = y * width + x;
	T px = input[px_idx];
	
	const float fx = x;
	const float fy = y;
	
	for( int n=0; n < numDetections; n++ )
	{
		const yoloNet::Detection det = detections[n];

		// check if this pixel is inside the bounding box
		if( fx >= det.Left && fx <= det.Right && fy >= det.Top && fy <= det.Bottom )
		{
			const float4 color = colors[det.ClassID];	

			const float alpha = color.w / 255.0f;
			const float ialph = 1.0f - alpha;

			px.x = alpha * color.x + ialph * px.x;
			px.y = alpha * color.y + ialph * px.y;
			px.z = alpha * color.z + ialph * px.z;
		}
	}
	
	output[px_idx] = px;	 
}


template<typename T>
__global__ void gpuDetectionOverlayBox( T* input, T* output, int imgWidth, int imgHeight, int x0, int y0, int boxWidth, int boxHeight, const float4 color ) 
{
	const int box_x = blockIdx.x * blockDim.x + threadIdx.x;
	const int box_y = blockIdx.y * blockDim.y + threadIdx.y;

	if( box_x >= boxWidth || box_y >= boxHeight )
		return;

	const int x = box_x + x0;
	const int y = box_y + y0;

	if( x >= imgWidth || y >= imgHeight )
		return;

	T px = input[ y * imgWidth + x ];

	const float alpha = color.w / 255.0f;
	const float ialph = 1.0f - alpha;

	px.x = alpha * color.x + ialph * px.x;
	px.y = alpha * color.y + ialph * px.y;
	px.z = alpha * color.z + ialph * px.z;
	
	output[y * imgWidth + x] = px;
}

template<typename T>
__global__ void gpuLetterbox(T* input, int input_width, int input_height,
                               T* output, int output_width, int output_height,
                               float scale, int pad_x, int pad_y, int new_width, int new_height,
                               float3 pad_color, int channels)
{
    const int x = blockIdx.x * blockDim.x + threadIdx.x;
    const int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x >= output_width || y >= output_height)
        return;
    
    // Check if pixel is in padded area
    if (x < pad_x || x >= pad_x + new_width || 
        y < pad_y || y >= pad_y + new_height) {
        // Set padding color
        for (int c = 0; c < channels; c++) {
            int idx = y * output_width * channels + x * channels + c;
            if (c == 0) output[idx] = pad_color.x;
            else if (c == 1) output[idx] = pad_color.y;
            else if (c == 2) output[idx] = pad_color.z;
            else output[idx] = 255; // Alpha channel
        }
    } else {
        // Map to input coordinates
        float src_x = (x - pad_x) / scale;
        float src_y = (y - pad_y) / scale;
        
        int src_x_int = (int)src_x;
        int src_y_int = (int)src_y;
        
        // Bounds check
        if (src_x_int >= 0 && src_x_int < input_width && 
            src_y_int >= 0 && src_y_int < input_height) {
            
            for (int c = 0; c < channels; c++) {
                int src_idx = src_y_int * input_width * channels + src_x_int * channels + c;
                int dst_idx = y * output_width * channels + x * channels + c;
                output[dst_idx] = input[src_idx];
            }
        }
    }
}

template<typename T>
cudaError_t launchDetectionOverlay( T* input, T* output, uint32_t width, uint32_t height, yoloNet::Detection* detections, int numDetections, float4* colors )
{
	if( !input || !output || width == 0 || height == 0 || !detections || numDetections == 0 || !colors )
		return cudaErrorInvalidValue;
			
	// this assumes that the output already has the input image copied to it,
	// which if input != output, is done first by yoloNet::Detect()
	for( int n=0; n < numDetections; n++ )
	{
		const int boxWidth = (int)detections[n].Width();
		const int boxHeight = (int)detections[n].Height();

		// launch kernel
		const dim3 blockDim(8, 8);
		const dim3 gridDim(iDivUp(boxWidth,blockDim.x), iDivUp(boxHeight,blockDim.y));

		float4 color = colors[detections[n].ClassID];
		
		if( detections[n].TrackID >= 0 )
			color.w *= 1.0f - (fminf(detections[n].TrackLost, 15.0f) / 15.0f);
			
		gpuDetectionOverlayBox<T><<<gridDim, blockDim>>>(input, output, width, height, (int)detections[n].Left, (int)detections[n].Top, boxWidth, boxHeight, color); 
	}

	return cudaGetLastError();
}

cudaError_t cudaDetectionOverlay( void* input, void* output, uint32_t width, uint32_t height, imageFormat format, yoloNet::Detection* detections, int numDetections, float4* colors )
{
	if( format == IMAGE_RGB8 )
		return launchDetectionOverlay<uchar3>((uchar3*)input, (uchar3*)output, width, height, detections, numDetections, colors); 
	else if( format == IMAGE_RGBA8 )
		return launchDetectionOverlay<uchar4>((uchar4*)input, (uchar4*)output, width, height, detections, numDetections, colors);  
	else if( format == IMAGE_RGB32F )
		return launchDetectionOverlay<float3>((float3*)input, (float3*)output, width, height, detections, numDetections, colors);  
	else if( format == IMAGE_RGBA32F )
		return launchDetectionOverlay<float4>((float4*)input, (float4*)output, width, height, detections, numDetections, colors); 
	else
		return cudaErrorInvalidValue;
}


cudaError_t cudaLetterbox(void* input, imageFormat input_format, 
                         uint32_t input_width, uint32_t input_height,
                         void* output, imageFormat output_format,
                         uint32_t output_width, uint32_t output_height,
                         const float3& pad_color,
                         yoloNet::LetterboxInfo* info,
                         cudaStream_t stream)
{
    if (!input || !output)
        return cudaErrorInvalidValue;
    
    // Calculate letterbox parameters
    float scale = std::min((float)output_width / input_width, 
                          (float)output_height / input_height);
    
    int new_width = (int)(input_width * scale);
    int new_height = (int)(input_height * scale);
    
    int pad_x = (output_width - new_width) / 2;
    int pad_y = (output_height - new_height) / 2;
    
    // Store info if requested
    if (info) {
        info->scale = scale;
        info->pad_x = pad_x;
        info->pad_y = pad_y;
        info->new_width = new_width;
        info->new_height = new_height;
    }
    
    // Get number of channels
    int channels = imageFormatChannels(input_format);
    
    // Launch kernel
    dim3 blockSize(16, 16);
    dim3 gridSize((output_width + blockSize.x - 1) / blockSize.x,
                  (output_height + blockSize.y - 1) / blockSize.y);
    
    if (input_format == IMAGE_RGB8 || input_format == IMAGE_RGBA8) {
        gpuLetterbox<uint8_t><<<gridSize, blockSize, 0, stream>>>(
            (uint8_t*)input, input_width, input_height,
            (uint8_t*)output, output_width, output_height,
            scale, pad_x, pad_y, new_width, new_height,
            pad_color, channels);
    } else if (input_format == IMAGE_RGB32F || input_format == IMAGE_RGBA32F) {
        gpuLetterbox<float><<<gridSize, blockSize, 0, stream>>>(
            (float*)input, input_width, input_height,
            (float*)output, output_width, output_height,
            scale, pad_x, pad_y, new_width, new_height,
            make_float3(pad_color.x/255.0f, pad_color.y/255.0f, pad_color.z/255.0f), channels);
    } else {
        return cudaErrorInvalidValue;
    }
    
    return cudaGetLastError();
}