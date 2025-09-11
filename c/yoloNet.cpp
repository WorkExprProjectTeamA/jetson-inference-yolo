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

#include "commandLine.h"
#include "filesystem.h"
#include "logging.h"

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
