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
 
#ifndef __YOLO_NET_H__
#define __YOLO_NET_H__


#include "tensorNet.h"

/**
 * Name of default input blob for YOLO model.
 * @ingroup yoloNet
 */
#define YOLONET_DEFAULT_INPUT   "images"

/**
 * Name of default output blob for YOLO model.
 * @ingroup yoloNet
 */
#define YOLONET_DEFAULT_OUTPUT  "output0"

/**
 * Default value of the minimum detection threshold
 * @ingroup yoloNet
 */
#define YOLONET_DEFAULT_CONFIDENCE_THRESHOLD 0.5f

/**
 * Default value of the clustering area-of-overlap threshold
 * @ingroup yoloNet
 */
#define YOLONET_DEFAULT_CLUSTERING_THRESHOLD 0.75f

/**
 * Default value of the minimum detection threshold
 * @deprecated please use YOLONET_DEFAULT_CONFIDENCE_THRESHOLD instead
 * @ingroup yoloNet
 */
#define YOLONET_DEFAULT_THRESHOLD  YOLONET_DEFAULT_CONFIDENCE_THRESHOLD

/**
 * Default alpha blending value used during overlay
 * @ingroup yoloNet
 */
#define YOLONET_DEFAULT_ALPHA 120

/**
 * The model type for yoloNet in data/networks/models.json
 * @ingroup yoloNet
 */
#define YOLONET_MODEL_TYPE "detection"

/**
 * Standard command-line options able to be passed to detectNet::Create()
 * @ingroup detectNet
 */
#define YOLONET_USAGE_STRING  "yoloNet arguments: \n" 					\
		  "  --network=NETWORK     pre-trained model to load, one of the following:\n" 		\
		  "                            * ssd-mobilenet-v1\n" 							\
		  "                            * ssd-mobilenet-v2 (default)\n" 					\
		  "                            * ssd-inception-v2\n" 							\
		  "                            * peoplenet\n"                                        \
		  "                            * peoplenet-pruned\n"                                 \
		  "                            * dashcamnet\n"                                       \
		  "                            * trafficcamnet\n"                                    \
		  "                            * facedetect\n"                                       \
		  "  --model=MODEL         path to custom model to load (caffemodel, uff, or onnx)\n" 					\
		  "  --prototxt=PROTOTXT   path to custom prototxt to load (for .caffemodel only)\n" 					\
		  "  --labels=LABELS       path to text file containing the labels for each class\n" 					\
		  "  --input-blob=INPUT    name of the input layer (default is '" YOLONET_DEFAULT_INPUT "')\n" 			\
		  "  --output-blob=OUTPUT name of the output layer (default is '" YOLONET_DEFAULT_OUTPUT "')\n" 	\
		  "  --mean-pixel=PIXEL    mean pixel value to subtract from input (default is 0.0)\n"					\
		  "  --confidence=CONF     minimum confidence threshold for detection (default is 0.5)\n"		           	\
		  "  --clustering=CLUSTER  minimum overlapping area threshold for clustering (default is 0.75)\n"             \
            "  --alpha=ALPHA         overlay alpha blending value, range 0-255 (default: 120)\n"					\
		  "  --overlay=OVERLAY     detection overlay flags (e.g. --overlay=box,labels,conf)\n"					\
		  "                        valid combinations are:  'box', 'lines', 'labels', 'conf', 'none'\n"			\
		  "  --profile             enable layer profiling in TensorRT\n\n"				\



// forward declarations
class objectTracker;

/**
 * YOLO with TensorRT support.
 * @ingroup yoloNet
 */
class yoloNet : public tensorNet
{
public:
	/**
	 * Object Detection result.
	 */
	struct Detection
	{
		// Detection Info
		uint32_t ClassID;	/**< Class index of the detected object. */
		float Confidence;	/**< Confidence value of the detected object. */

		// Tracking Info
		int TrackID;		/**< Unique tracking ID (or -1 if untracked) */
		int TrackStatus;	/**< -1 for dropped, 0 for initializing, 1 for active/valid */ 
		int TrackFrames;	/**< The number of frames the object has been re-identified for */
		int TrackLost;   	/**< The number of consecutive frames tracking has been lost for */
		
		// Bounding Box Coordinates
		float Left;		/**< Left bounding box coordinate (in pixels) */
		float Right;		/**< Right bounding box coordinate (in pixels) */
		float Top;		/**< Top bounding box cooridnate (in pixels) */
		float Bottom;		/**< Bottom bounding box coordinate (in pixels) */

		/**< Calculate the width of the object */
		inline float Width() const											{ return Right - Left; }

		/**< Calculate the height of the object */
		inline float Height() const											{ return Bottom - Top; }

		/**< Calculate the area of the object */
		inline float Area() const											{ return Width() * Height(); }

		/**< Calculate the width of the bounding box */
		static inline float Width( float x1, float x2 )							{ return x2 - x1; }

		/**< Calculate the height of the bounding box */
		static inline float Height( float y1, float y2 )							{ return y2 - y1; }

		/**< Calculate the area of the bounding box */
		static inline float Area( float x1, float y1, float x2, float y2 )			{ return Width(x1,x2) * Height(y1,y2); }

		/**< Return the center of the object */
		inline void Center( float* x, float* y ) const							{ if(x) *x = Left + Width() * 0.5f; if(y) *y = Top + Height() * 0.5f; }

		/**< Return true if the coordinate is inside the bounding box */
		inline bool Contains( float x, float y ) const							{ return x >= Left && x <= Right && y >= Top && y <= Bottom; }
		
		/**< Return true if the bounding boxes intersect and exceeds area % threshold */	
		inline bool Intersects( const Detection& det, float areaThreshold=0.0f ) const  { return (IntersectionArea(det) / fmaxf(Area(), det.Area()) > areaThreshold); }
	
		/**< Return true if the bounding boxes intersect and exceeds area % threshold */	
		inline bool Intersects( float x1, float y1, float x2, float y2, float areaThreshold=0.0f ) const  { return (IntersectionArea(x1,y1,x2,y2) / fmaxf(Area(), Area(x1,y1,x2,y2)) > areaThreshold); }
	
		/**< Return the area of the bounding box intersection */
		inline float IntersectionArea( const Detection& det ) const					{ return IntersectionArea(det.Left, det.Top, det.Right, det.Bottom); }

		/**< Return the area of the bounding box intersection */
		inline float IntersectionArea( float x1, float y1, float x2, float y2 ) const	{ if(!Overlaps(x1,y1,x2,y2)) return 0.0f; return (fminf(Right, x2) - fmaxf(Left, x1)) * (fminf(Bottom, y2) - fmaxf(Top, y1)); }

		/**< Calculate the Intersection-Over-Union (IOU) ratio */
		inline float IOU( const Detection& det ) const							{ return IOU(det.Left, det.Top, det.Right, det.Bottom); }
		
		/**< Calculate the Intersection-Over-Union (IOU) ratio */
		inline float IOU( float x1, float y1, float x2, float y2 ) const;
		
		/**< Return true if the bounding boxes overlap */
		inline bool Overlaps( const Detection& det ) const						{ return !(det.Left > Right || det.Right < Left || det.Top > Bottom || det.Bottom < Top); }
		
		/**< Return true if the bounding boxes overlap */
		inline bool Overlaps( float x1, float y1, float x2, float y2 ) const			{ return !(x1 > Right || x2 < Left || y1 > Bottom || y2 < Top); }
		
		/**< Expand the bounding box if they overlap (return true if so) */
		inline bool Expand( float x1, float y1, float x2, float y2 ) 	     		{ if(!Overlaps(x1, y1, x2, y2)) return false; Left = fminf(x1, Left); Top = fminf(y1, Top); Right = fmaxf(x2, Right); Bottom = fmaxf(y2, Bottom); return true; }
		
		/**< Expand the bounding box if they overlap (return true if so) */
		inline bool Expand( const Detection& det )      							{ if(!Overlaps(det)) return false; Left = fminf(det.Left, Left); Top = fminf(det.Top, Top); Right = fmaxf(det.Right, Right); Bottom = fmaxf(det.Bottom, Bottom); return true; }

		/**< Reset all member variables to zero */
		inline void Reset()													{ ClassID = 0; Confidence = 0; TrackID = -1; TrackStatus = -1; TrackFrames = 0; TrackLost = 0; Left = 0; Right = 0; Top = 0; Bottom = 0; } 								

		/**< Default constructor */
		inline Detection()													{ Reset(); }
	};

	/**
	 * Overlay flags (can be OR'd together).
	 */
	enum OverlayFlags
	{
		OVERLAY_NONE       = 0,			/**< No overlay. */
		OVERLAY_BOX        = (1 << 0),	/**< Overlay the object bounding boxes (filled) */
		OVERLAY_LABEL 	    = (1 << 1),	/**< Overlay the class description labels */
		OVERLAY_CONFIDENCE = (1 << 2),	/**< Overlay the detection confidence values */
		OVERLAY_TRACKING   = (1 << 3),	/**< Overlay tracking information (like track ID) */
		OVERLAY_LINES      = (1 << 4),     /**< Overlay the bounding box lines (unfilled) */
		OVERLAY_DEFAULT    = OVERLAY_BOX|OVERLAY_LABEL|OVERLAY_CONFIDENCE, /**< The default choice of overlay */
	};

	/**
	 * Letterbox for pre/post processing
	*/
	struct LetterboxInfo {
          float scale;
          int pad_x;
          int pad_y;
          int new_width;
          int new_height;
    };

	/**
	 * Parse a string sequence into OverlayFlags enum.
	 * Valid flags are "none", "box", "label", and "conf" and it is possible to combine flags
	 * (bitwise OR) together with commas or pipe (|) symbol.  For example, the string sequence
	 * "box,label,conf" would return the flags `OVERLAY_BOX|OVERLAY_LABEL|OVERLAY_CONFIDENCE`.
	 */
	static uint32_t OverlayFlagsFromStr( const char* flags );

/**
	 * Load a custom network instance
	 * @param prototxt_path File path to the deployable network prototxt
	 * @param model_path File path to the caffemodel
	 * @param mean_pixel Input transform subtraction value (use 0.0 if the network already does this)
	 * @param class_labels File path to list of class name labels
	 * @param class_colors File path to list of class colors
	 * @param threshold default minimum threshold for detection
	 * @param input Name of the input layer blob.
	 * @param output Name of the output layer blob.
	 * @param maxBatchSize The maximum batch size that the network will support and be optimized for.
	 */
	static yoloNet* Create( const char* prototxt_path, const char* model_path, float mean_pixel, 
						 const char* class_labels, const char* class_colors,
						 float threshold=YOLONET_DEFAULT_CONFIDENCE_THRESHOLD, 
						 const char* input = YOLONET_DEFAULT_INPUT, 
						 const char* output = YOLONET_DEFAULT_OUTPUT,
						 uint32_t maxBatchSize=DEFAULT_MAX_BATCH_SIZE, 
						 precisionType precision=TYPE_FASTEST,
				   		 deviceType device=DEVICE_GPU, bool allowGPUFallback=true );

    static inline const char* Usage() 		{ return YOLONET_USAGE_STRING; }
    
    virtual ~yoloNet();

	/**
	 * Detect object locations from an image, returning an array containing the detection results.
	 * @param[in]  input input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[out] detections pointer that will be set to array of detection results (residing in shared CPU/GPU memory)
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    The number of detected objects, 0 if there were no detected objects, and -1 if an error was encountered.
	 */
	template<typename T> int Detect( T* image, uint32_t width, uint32_t height, Detection** detections, uint32_t overlay=OVERLAY_DEFAULT )		{ return Detect((void*)image, width, height, imageFormatFromType<T>(), detections, overlay); }

	/**
	 * Detect object locations from an image, returning an array containing the detection results.
	 * @param[in]  input input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[out] detections pointer that will be set to array of detection results (residing in shared CPU/GPU memory)
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    The number of detected objects, 0 if there were no detected objects, and -1 if an error was encountered.
	 */
	int Detect( void* input, uint32_t width, uint32_t height, imageFormat format, Detection** detections, uint32_t overlay=OVERLAY_DEFAULT );

	/**
	 * Detect object locations from an image, into an array of the results allocated by the user.
	 * @param[in]  input input image in CUDA device memory (uchar3/uchar4/float3/float4)
	 * @param[in]  width width of the input image in pixels.
	 * @param[in]  height height of the input image in pixels.
	 * @param[out] detections pointer to user-allocated array that will be filled with the detection results.
	 *             @see GetMaxDetections() for the number of detection results that should be allocated in this buffer.
	 * @param[in]  overlay bitwise OR combination of overlay flags (@see OverlayFlags and @see Overlay()), or OVERLAY_NONE.
	 * @returns    The number of detected objects, 0 if there were no detected objects, and -1 if an error was encountered.
	 */
	int Detect( void* input, uint32_t width, uint32_t height, imageFormat format, Detection* detections, uint32_t overlay=OVERLAY_DEFAULT );
	
	/**
	 * Draw the detected bounding boxes overlayed on an RGBA image.
	 * @note Overlay() will automatically be called by default by Detect(), if the overlay parameter is true 
	 * @param input input image in CUDA device memory.
	 * @param output output image in CUDA device memory.
	 * @param detections Array of detections allocated in CUDA device memory.
	 */
	template<typename T> bool Overlay( T* input, T* output, uint32_t width, uint32_t height, Detection* detections, uint32_t numDetections, uint32_t flags=OVERLAY_DEFAULT )			{ return Overlay(input, output, width, height, imageFormatFromType<T>(), detections, numDetections, flags); }
	
	/**
	 * Draw the detected bounding boxes overlayed on an RGBA image.
	 * @note Overlay() will automatically be called by default by Detect(), if the overlay parameter is true 
	 * @param input input image in CUDA device memory.
	 * @param output output image in CUDA device memory.
	 * @param detections Array of detections allocated in CUDA device memory.
	 */
	bool Overlay( void* input, void* output, uint32_t width, uint32_t height, imageFormat format, Detection* detections, uint32_t numDetections, uint32_t flags=OVERLAY_DEFAULT );

	/**
	 * Retrieve the maximum number of simultaneous detections the network supports.
	 * Knowing this is useful for allocating the buffers to store the output detection results.
	 */
	inline uint32_t GetMaxDetections() const					{ return mMaxDetections; } 

	/**
	 * Retrieve the description of a particular class.
	 */
	inline const char* GetClassDesc( uint32_t index )	const		{ if(index >= mClassDesc.size()) { printf("invalid class %u\n", index); return "Invalid"; } return mClassDesc[index].c_str(); }

    /**
	 * Set the minimum threshold for detection.
	 */
	inline void SetConfidenceThreshold( float threshold ) 			{ mConfidenceThreshold = threshold; }

protected:          
	// constructor
	yoloNet( float meanPixel=0.0f );

	bool allocDetections();

	bool loadClassInfo( const char* filename );
	bool loadClassColors( const char* filename );
	
	bool init( const char* prototxt_path, const char* model_path, 
             const char* class_labels, const char* class_colors,
			 float threshold, const char* input, const char* output,  
             uint32_t maxBatchSize, 
			 precisionType precision, deviceType device, bool allowGPUFallback );

	bool preProcess( void* input, uint32_t width, uint32_t height, imageFormat format );
	int postProcess( void* input, uint32_t width, uint32_t height, imageFormat format, Detection* detections );

	void scaleCoordinates( Detection* detections, int numDetections, uint32_t originalWidth, uint32_t originalHeight );
	int applyNMS( Detection* detections, int numDetections, float iouThreshold = 0.45f );

	int clusterDetections( Detection* detections, int n );
	void sortDetections( Detection* detections, int numDetections );

	objectTracker* mTracker;
	
	float mConfidenceThreshold;	 // TODO change this to per-class
	float mClusteringThreshold;	 // TODO change this to per-class
	
	float mMeanPixel;
	float mLineWidth;
	float mOverlayAlpha;
	
	float4* mClassColors;
	
	std::vector<std::string> mClassDesc;
	std::vector<std::string> mClassSynset;

	std::string mClassPath;
	uint32_t	  mNumClasses;

	Detection* mDetectionSets;	// list of detections, mNumDetectionSets * mMaxDetections
	uint32_t   mDetectionSet;	// index of next detection set to use
	uint32_t	 mMaxDetections;	// number of raw detections in the grid

	static const uint32_t mNumDetectionSets = 16; // size of detection ringbuffer

	LetterboxInfo mLastLetterboxInfo;

};


// bounding box IOU
inline float yoloNet::Detection::IOU( float x1, float y1, float x2, float y2 ) const		
{
	const float overlap_x0 = fmaxf(Left, x1);
	const float overlap_y0 = fmaxf(Top, y1);
	const float overlap_x1 = fminf(Right, x2);
	const float overlap_y1 = fminf(Bottom, y2);
	
	// check if there is an overlap
	if( (overlap_x1 - overlap_x0 <= 0) || (overlap_y1 - overlap_y0 <= 0) )
		return 0;
	
	// calculate the ratio of the overlap to each ROI size and the unified size
	const float size_1 = Area();
	const float size_2 = Area(x1, y1, x2, y2);
	
	const float size_intersection = (overlap_x1 - overlap_x0) * (overlap_y1 - overlap_y0);
	const float size_union = size_1 + size_2 - size_intersection;
	
	return size_intersection / size_union;
}


#endif