/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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
#include "objectTracker.h"
#include "tensorConvert.h"
#include "modelDownloader.h"

#include "cudaMappedMemory.h"
#include "cudaFont.h"
#include "cudaDraw.h"
#include "cudaResize.h"
#include "cudaColorspace.h"

#include "commandLine.h"
#include "filesystem.h"
#include "logging.h"

#include "imageIO.h"

#define CHECK_NULL_STR(x)	(x != NULL) ? x : "NULL"

// constructor
yoloNet::yoloNet( float meanPixel ) : tensorNet()
{
	mTracker   = NULL;
	mMeanPixel = meanPixel;
	mLineWidth = 2.0f;

	mNumClasses  = 0;
	mClassColors = NULL;
	
	mDetectionSets = NULL;
	mDetectionSet  = 0;
	mMaxDetections = 0;
	mOverlayAlpha  = YOLONET_DEFAULT_ALPHA;
	
	mConfidenceThreshold = YOLONET_DEFAULT_CONFIDENCE_THRESHOLD;
	mClusteringThreshold = YOLONET_DEFAULT_CLUSTERING_THRESHOLD;
}


// destructor
yoloNet::~yoloNet()
{
	SAFE_DELETE(mTracker);
	
	CUDA_FREE_HOST(mDetectionSets);
	CUDA_FREE_HOST(mClassColors);
}

// init
bool yoloNet::init( const char* prototxt, const char* model, const char* class_labels, const char* class_colors,
			 	  float threshold, const char* input_blob, const char* output_blob,
				  uint32_t maxBatchSize, precisionType precision, deviceType device, bool allowGPUFallback )
{
	LogInfo("\n");
	LogInfo("yoloNet -- loading detection network model from:\n");
	LogInfo("          -- prototxt     %s\n", CHECK_NULL_STR(prototxt));
	LogInfo("          -- model        %s\n", CHECK_NULL_STR(model));
	LogInfo("          -- input_blob   '%s'\n", CHECK_NULL_STR(input_blob));
	LogInfo("          -- output_blob   '%s'\n", CHECK_NULL_STR(output_blob));
	LogInfo("          -- mean_pixel   %f\n", mMeanPixel);
	LogInfo("          -- class_labels %s\n", CHECK_NULL_STR(class_labels));
	LogInfo("          -- class_colors %s\n", CHECK_NULL_STR(class_colors));
	LogInfo("          -- threshold    %f\n", threshold);
	LogInfo("          -- batch_size   %u\n\n", maxBatchSize);

	// create list of output names	
	std::vector<std::string> output_blobs;

	output_blobs.push_back(output_blob);
	
	// ONNX SSD models require larger workspace size
	if( modelTypeFromPath(model) == MODEL_ONNX )
	{
		size_t gpuMemFree = 0;
		size_t gpuMemTotal = 0;
		
		CUDA(cudaMemGetInfo(&gpuMemFree, &gpuMemTotal));

		if( gpuMemTotal <= (2048 << 20) )
			mWorkspaceSize = 512 << 20;
		else
			mWorkspaceSize = 2048 << 20;
	}

	// load the model
	if( !LoadNetwork(prototxt, model, NULL, input_blob, output_blobs, 
				  maxBatchSize, precision, device, allowGPUFallback) )
	{
		LogError(LOG_TRT "detectNet -- failed to initialize.\n");
		return false;
	}
	
	// allocate detection sets
	if( !allocDetections() )
		return false;

	// load class descriptions
	loadClassInfo(class_labels);
	loadClassColors(class_colors);

	// set the specified threshold
	SetConfidenceThreshold(threshold);

	return true;
}

// Create
yoloNet* yoloNet::Create( const char* prototxt, const char* model, float mean_pixel, 
						const char* class_labels, const char* class_colors, float threshold,
						const char* input_blob, const char* output_blob,
						uint32_t maxBatchSize, precisionType precision, deviceType device, bool allowGPUFallback )
{
	// load custom model
    yoloNet* net = new yoloNet(mean_pixel);
	
	if( !net )
		return NULL;

	if( !net->init(prototxt, model, class_labels, class_colors, threshold, input_blob, output_blob,
				maxBatchSize, precision, device, allowGPUFallback) )
		return NULL;

	return net;
}

// allocDetections
bool yoloNet::allocDetections()
{
	mNumClasses = DIMS_H(mOutputs[0].dims) - 4;
	mMaxDetections = DIMS_W(mOutputs[0].dims) /** mNumClasses*/;
	LogInfo(LOG_TRT "yoloNet -- number of object classes: %u\n", mNumClasses);

	LogVerbose(LOG_TRT "yoloNet -- maximum bounding boxes:   %u\n", mMaxDetections);

	// allocate array to store detection results
	const size_t det_size = sizeof(Detection) * mNumDetectionSets * mMaxDetections;
	
	if( !cudaAllocMapped((void**)&mDetectionSets, det_size) )
		return false;
	
	memset(mDetectionSets, 0, det_size);
	return true;
}

// loadClassInfo
bool yoloNet::loadClassInfo( const char* filename )
{
	if( !LoadClassLabels(filename, mClassDesc, mClassSynset, mNumClasses) )
		return false;

	if( IsModelType(MODEL_UFF) )
		mNumClasses = mClassDesc.size();

	LogInfo(LOG_TRT "yoloNet -- number of object classes:  %u\n", mNumClasses);
	
	if( filename != NULL )
		mClassPath = locateFile(filename);	
	
	return true;
}


// loadClassColors
bool yoloNet::loadClassColors( const char* filename )
{
	return LoadClassColors(filename, &mClassColors, mNumClasses, YOLONET_DEFAULT_ALPHA);
}


int yoloNet::Detect( void* input, uint32_t width, uint32_t height, imageFormat format, Detection** detections, uint32_t overlay )
{
	Detection* det = mDetectionSets + mDetectionSet * GetMaxDetections();

	if( detections != NULL )
		*detections = det;

	mDetectionSet++;

	if( mDetectionSet >= mNumDetectionSets )
		mDetectionSet = 0;
	
	return Detect(input, width, height, format, det, overlay);
}


// Detect
int yoloNet::Detect( void* input, uint32_t width, uint32_t height, imageFormat format, Detection* detections, uint32_t overlay )
{
	// verify parameters
	if( !input || width == 0 || height == 0 || !detections )
	{
		LogError(LOG_TRT "detectNet::Detect( 0x%p, %u, %u ) -> invalid parameters\n", input, width, height);
		return -1;
	}
	
	if( !imageFormatIsRGB(format) )
	{
		LogError(LOG_TRT "detectNet::Detect() -- unsupported image format (%s)\n", imageFormatToStr(format));
		LogError(LOG_TRT "                       supported formats are:\n");
		LogError(LOG_TRT "                          * rgb8\n");		
		LogError(LOG_TRT "                          * rgba8\n");		
		LogError(LOG_TRT "                          * rgb32f\n");		
		LogError(LOG_TRT "                          * rgba32f\n");

		return false;
	}
	
	if( !preProcess(input, width, height, format) )
		return -1;

	if( !ProcessNetwork() )
		return -1;

	std::vector<int> totalDetections = postProcess(detections);
	const int numDetections = totalDetections[0];
	const int numAlerts = totalDetections[1];

	// render the overlay
	if( overlay != 0 && numAlerts > 0 )
	{
		if( !Overlay(input, input, width, height, format, detections, numDetections, overlay) )
			LogError(LOG_TRT "detectNet::Detect() -- failed to render overlay\n");
	}
	
	// wait for GPU to complete work			
	//CUDA(cudaDeviceSynchronize());	// BUG is this needed here?

	// return the number of detections
	return numDetections;
}

// letterbox function
bool Letterbox(cv::Mat& input, uint32_t input_width, uint32_t input_height, imageFormat input_format, cv::Mat& output, yoloNet::PreParam* pparam, uint32_t model_width, uint32_t model_height)
{
	const float inp_w = model_width;
	const float inp_h = model_height;

    float r    = std::min(inp_h / input_height, inp_w / input_width);
    int   padw = std::round(input_width * r);
    int   padh = std::round(input_height * r);

    cv::Mat tmp;
    if ((int)input_width != padw || (int)input_height != padh) {
        cv::resize(input, tmp, cv::Size(padw, padh));
    }
    else {
        tmp = input.clone();
    }

    float dw = inp_w - padw;
    float dh = inp_h - padh;

    dw /= 2.0f;
    dh /= 2.0f;
    int top    = int(std::round(dh - 0.1f));
    int bottom = int(std::round(dh + 0.1f));
    int left   = int(std::round(dw - 0.1f));
    int right  = int(std::round(dw + 0.1f));

    cv::copyMakeBorder(tmp, tmp, top, bottom, left, right, cv::BORDER_CONSTANT, {114, 114, 114});

    cv::dnn::blobFromImage(tmp, output, 1 / 255.f, cv::Size(), cv::Scalar(0, 0, 0), true, false, CV_32F);

    pparam->ratio  = 1 / r;
    pparam->dw     = dw;
    pparam->dh     = dh;
    pparam->height = input_height;
    pparam->width  = input_width;

    return true;
}

// preProcess
bool yoloNet::preProcess( void* input, uint32_t width, uint32_t height, imageFormat format )
{
	PROFILER_BEGIN(PROFILER_PREPROCESS);

	cv::Mat inputMat;
	if (format == IMAGE_RGB8) {
		inputMat = cv::Mat(height, width, CV_8UC3, input);
	} else {
		LogError("Unsupported format for cv::Mat conversion\n");
      return false;
	}

	cv::Mat nchw;

	Letterbox(inputMat, width, height, format, nchw, &mPreParam, GetInputWidth(), GetInputHeight());

    if (CUDA_FAILED(cudaMemcpy(mInputs[0].CUDA, nchw.data, nchw.total() * nchw.elemSize(), cudaMemcpyHostToDevice))) 
	{
		LogError(LOG_TRT "yoloNet::preProcess() -- failed to copy nchw data to GPU\n");
      	return false;
  	}


    PROFILER_END(PROFILER_PREPROCESS);
    return true;
}


// postProcess
std::vector<int> yoloNet::postProcess( Detection* detections )
{
	PROFILER_BEGIN(PROFILER_POSTPROCESS);

	int numDetections = 0;
	int numAlerts = 0;

	mOutputs[0].CPU = (float*)malloc(mOutputs[0].size);

	CUDA(cudaMemcpy(mOutputs[0].CPU, mOutputs[0].CUDA, mOutputs[0].size, cudaMemcpyDeviceToHost));

	float* output_host_ptr = mOutputs[0].CPU;  // YOLO output: (1, 60, 8400)

	const uint32_t numAnchors = DIMS_W(mOutputs[0].dims);  // 8400
	const uint32_t numChannels = DIMS_H(mOutputs[0].dims); // 60
	const uint32_t numClasses = numChannels - 4;

	std::vector<cv::Rect> bboxes;
    std::vector<float>    scores;
    std::vector<int>      labels;
    std::vector<int>      indices;

	cv::Mat output = cv::Mat(numChannels, numAnchors, CV_32F, output_host_ptr);
    output         = output.t();

	for (int i = 0; i < numAnchors; i++) {
        auto  row_ptr    = output.row(i).ptr<float>();
        auto  bboxes_ptr = row_ptr;
        auto  scores_ptr = row_ptr + 4;
        auto  max_s_ptr  = std::max_element(scores_ptr, scores_ptr + numClasses);
        float score      = *max_s_ptr;
        if (score > YOLONET_SCORE_THRESHOLD) {
            float x = *bboxes_ptr++ - mPreParam.dw;
            float y = *bboxes_ptr++ - mPreParam.dh;
            float w = *bboxes_ptr++;
            float h = *bboxes_ptr;

            float x0 = clamp((x - 0.5f * w) * mPreParam.ratio, 0.f, mPreParam.width);
            float y0 = clamp((y - 0.5f * h) * mPreParam.ratio, 0.f, mPreParam.height);
            float x1 = clamp((x + 0.5f * w) * mPreParam.ratio, 0.f, mPreParam.width);
            float y1 = clamp((y + 0.5f * h) * mPreParam.ratio, 0.f, mPreParam.height);

            int              label = max_s_ptr - scores_ptr;
            cv::Rect_<float> bbox;
            bbox.x      = x0;
            bbox.y      = y0;
            bbox.width  = x1 - x0;
            bbox.height = y1 - y0;

            bboxes.push_back(bbox);
            labels.push_back(label);
            scores.push_back(score);
        }
	}

	cv::dnn::NMSBoxesBatched(bboxes, scores, labels, YOLONET_SCORE_THRESHOLD, YOLONET_IOU_THRESHOLD, indices);

    for (auto& i : indices) {
        if (numDetections >= YOLONET_TOPK) {
            break;
        }
        detections[numDetections].ClassID = labels[i];
		detections[numDetections].Confidence = scores[i];
		detections[numDetections].Left = bboxes[i].x;
		detections[numDetections].Right = bboxes[i].x + bboxes[i].width;
		detections[numDetections].Top = bboxes[i].y;
		detections[numDetections].Bottom = bboxes[i].y + bboxes[i].height;

		if (mAlertClasses.find(labels[i]) != mAlertClasses.end()) {
			numAlerts += 1;
		}
        numDetections += 1;
    }

    PROFILER_END(PROFILER_POSTPROCESS);
    
	std::vector<int> totalDetections;
	totalDetections.push_back(numDetections);
	totalDetections.push_back(numAlerts);

	return totalDetections;
}

// sortDetections (by area)
void yoloNet::sortDetections( Detection* detections, int numDetections )
{
	if( numDetections < 2 )
		return;

	// order by area (descending) or confidence (ascending)
	for( int i=0; i < numDetections-1; i++ )
	{
		for( int j=0; j < numDetections-i-1; j++ )
		{
			if( detections[j].Area() < detections[j+1].Area() ) //if( detections[j].Confidence > detections[j+1].Confidence )
			{
				const Detection det = detections[j];
				detections[j] = detections[j+1];
				detections[j+1] = det;
			}
		}
	}

	// renumber the instance ID's
	//for( int i=0; i < numDetections; i++ )
	//	detections[i].TrackID = i;	
}

// from yoloNet.cu
cudaError_t cudaDetectionOverlay( void* input, void* output, uint32_t width, uint32_t height, imageFormat format, yoloNet::Detection* detections, int numDetections, float4* colors );

// Overlay
bool yoloNet::Overlay( void* input, void* output, uint32_t width, uint32_t height, imageFormat format, Detection* detections, uint32_t numDetections, uint32_t flags )
{
	PROFILER_BEGIN(PROFILER_VISUALIZE);

	if( flags == 0 )
	{
		LogError(LOG_TRT "detectNet -- Overlay() was called with OVERLAY_NONE, returning false\n");
		return false;
	}

	uchar3* device_ptr;

	size_t imageSize = imageFormatSize(format, width, height);

	if( CUDA_FAILED(cudaMalloc((void**)&device_ptr, imageSize)) )
  	{
      	LogError(LOG_TRT "detectNet -- Overlay() failed to allocate GPU memory\n");
      	return false;
  	}
	CUDA(cudaMemset(device_ptr, 0, imageSize));
	// if input and output are different images, copy the input to the output first
	// then overlay the bounding boxes, ect. on top of the output image
	// if( input != output )
	// {
	// if( CUDA_FAILED(cudaMemcpy(output, input, imageFormatSize(format, width, height), cudaMemcpyDeviceToDevice)) )
	if (CUDA_FAILED(cudaMemcpy(device_ptr, input, imageSize, cudaMemcpyHostToDevice)))
	{
		LogError(LOG_TRT "detectNet -- Overlay() failed to copy input image to output image\n");
		return false;
	}
	// }

	// make sure there are actually detections
	if( numDetections <= 0 )
	{
		PROFILER_END(PROFILER_VISUALIZE);
		return true;
	}

	// bounding box overlay
	if( flags & OVERLAY_BOX )
	{
		if( CUDA_FAILED(cudaDetectionOverlay(device_ptr, device_ptr, width, height, format, detections, numDetections, mClassColors)) )
			return false;
	}
	
	// bounding box lines
	if( flags & OVERLAY_LINES )
	{
		for( uint32_t n=0; n < numDetections; n++ )
		{
			const Detection* d = detections + n;
			const float4& color = mClassColors[d->ClassID];

			CUDA(cudaDrawLine(device_ptr, device_ptr, width, height, format, d->Left, d->Top, d->Right, d->Top, color, mLineWidth));
			CUDA(cudaDrawLine(device_ptr, device_ptr, width, height, format, d->Right, d->Top, d->Right, d->Bottom, color, mLineWidth));
			CUDA(cudaDrawLine(device_ptr, device_ptr, width, height, format, d->Left, d->Bottom, d->Right, d->Bottom, color, mLineWidth));
			CUDA(cudaDrawLine(device_ptr, device_ptr, width, height, format, d->Left, d->Top, d->Left, d->Bottom, color, mLineWidth));
		}
	}
			
	// class label overlay
	if( (flags & OVERLAY_LABEL) || (flags & OVERLAY_CONFIDENCE) || (flags & OVERLAY_TRACKING) )
	{
		static cudaFont* font = NULL;

		// make sure the font object is created
		if( !font )
		{
			font = cudaFont::Create(adaptFontSize(width));  // 20.0f
	
			if( !font )
			{
				LogError(LOG_TRT "detectNet -- Overlay() was called with OVERLAY_FONT, but failed to create cudaFont()\n");
				return false;
			}
		}

		// draw each object's description
	#ifdef BATCH_TEXT
		std::vector<std::pair<std::string, int2>> labels;
	#endif 
		for( uint32_t n=0; n < numDetections; n++ )
		{
			const char* className  = GetClassDesc(detections[n].ClassID);
			const float confidence = detections[n].Confidence * 100.0f;
			const int2  position   = make_int2(detections[n].Left+5, detections[n].Top+3);
			
			char buffer[256];
			char* str = buffer;
			
			if( flags & OVERLAY_LABEL )
				str += sprintf(str, "%s ", className);
			
			if( flags & OVERLAY_TRACKING && detections[n].TrackID >= 0 )
				str += sprintf(str, "%i ", detections[n].TrackID);
			
			if( flags & OVERLAY_CONFIDENCE )
				str += sprintf(str, "%.1f%%", confidence);

		#ifdef BATCH_TEXT
			labels.push_back(std::pair<std::string, int2>(buffer, position));
		#else
			float4 color = make_float4(255,255,255,255);
		
			if( detections[n].TrackID >= 0 )
				color.w *= 1.0f - (fminf(detections[n].TrackLost, 15.0f) / 15.0f);
			
			font->OverlayText(device_ptr, format, width, height, buffer, position.x, position.y, color);
		#endif
		}

	#ifdef BATCH_TEXT
		font->OverlayText(device_ptr, format, width, height, labels, make_float4(255,255,255,255));
	#endif
	}

	if(CUDA_FAILED(cudaMemcpy(output, device_ptr, imageSize, cudaMemcpyDeviceToHost)) )
	{
		LogError(LOG_TRT "detectNet -- Overlay() failed to copy GPU to output\n");
		cudaFree(device_ptr);
		return false;
	}
	
	PROFILER_END(PROFILER_VISUALIZE);
	return true;
}


// OverlayFlagsFromStr
uint32_t yoloNet::OverlayFlagsFromStr( const char* str_user )
{
	if( !str_user )
		return OVERLAY_DEFAULT;

	// copy the input string into a temporary array,
	// because strok modifies the string
	const size_t str_length = strlen(str_user);
	const size_t max_length = 256;
	
	if( str_length == 0 )
		return OVERLAY_DEFAULT;

	if( str_length >= max_length )
	{
		LogError(LOG_TRT "yoloNet::OverlayFlagsFromStr() overlay string exceeded max length of %zu characters ('%s')", max_length, str_user);
		return OVERLAY_DEFAULT;
	}
	
	char str[max_length];
	strcpy(str, str_user);

	// tokenize string by delimiters ',' and '|'
	const char* delimiters = ",|";
	char* token = strtok(str, delimiters);

	if( !token )
		return OVERLAY_DEFAULT;

	// look for the tokens:  "box", "label", "default", and "none"
	uint32_t flags = OVERLAY_NONE;

	while( token != NULL )
	{
		if( strcasecmp(token, "box") == 0 )
			flags |= OVERLAY_BOX;
		else if( strcasecmp(token, "label") == 0 || strcasecmp(token, "labels") == 0 )
			flags |= OVERLAY_LABEL;
		else if( strcasecmp(token, "conf") == 0 || strcasecmp(token, "confidence") == 0 )
			flags |= OVERLAY_CONFIDENCE;
		else if( strcasecmp(token, "track") == 0 || strcasecmp(token, "tracking") == 0 )
			flags |= OVERLAY_TRACKING;
		else if( strcasecmp(token, "line") == 0 || strcasecmp(token, "lines") == 0 )
			flags |= OVERLAY_LINES;
		else if( strcasecmp(token, "default") == 0 )
			flags |= OVERLAY_DEFAULT;

		token = strtok(NULL, delimiters);
	}	

	return flags;
}
