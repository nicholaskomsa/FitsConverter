#pragma once

#define FREEIMAGE_LIB
#include <FreeImage.h>
#include <fitsio.h>

#include <string>
#include <span>
#include <vector>
#include <execution>
#include <algorithm>
#include <format>

namespace FitsConverter {

	void floatSpaceConvert(const std::span<float> data, std::span<uint32_t> converted, bool nickRgbMode = true, double vMin = 0.0, double vMax = 1.0, double stripeNum = 1) {

		auto getViewWindow = [&](double startPercent = 0.0, double endPercent = 1.0) ->std::tuple<double, double, double> {

			double min = *std::min_element(data.begin(), data.end());
			double max = *std::max_element(data.begin(), data.end());
			double distance = max - min;

			double viewMin = min + distance * startPercent;
			double viewMax = min + distance * endPercent;
			double viewDistance = viewMax - viewMin;

			if (viewDistance == 0) viewDistance = 1;

			return { viewMin, viewMax, viewDistance };
		};

		auto [viewMin, viewMax, viewDistance] = getViewWindow(vMin, vMax);	//0,1 is full view window of data

		double stripeDistance = viewDistance / stripeNum;

		auto convertToGreyScale = [&](double f)->double {

			f = std::min(f, viewMax);
			f = std::max(f, viewMin);

			double percent = 1.0;

			f -= viewMin;

			if (f < viewDistance) {
				f -= stripeDistance * std::floor(f / stripeDistance);

				percent = f / stripeDistance;
			}

			//percent is between 0 and 1
			return percent;
		};

		if (nickRgbMode) {

			uint32_t intmax = std::numeric_limits<uint32_t>::max();//max will use alpha
			//get rid of alpha channel, rgb remains, a = 0 
			intmax <<= 8;
			intmax >>= 8;

			std::for_each(std::execution::par, data.begin(), data.end(), [&, front = data.data()](auto& f) {

				auto percent = convertToGreyScale(f);

				//this is the nickrgb transform
				uint32_t i = intmax * percent; 	//project onto integer	// int RGB

				//store pixel data
				std::size_t index = &f - front;
				converted[index] = i;
				//set the alpha to opaque
				reinterpret_cast<uint8_t*>(&converted[index])[3] = 255;
				});

		}
		else {
			//set to roygbiv
			std::for_each(std::execution::par, data.begin(), data.end(), [&, front = data.data()](auto& f) {

				auto percent = convertToGreyScale(f);

				//this is the roygbiv transform

				uint8_t r = 0, g = 0, b = 0;

				/*plot short rainbow RGB*/
				double a = (1.0 - percent) / 0.20;	//invert and group
				int X = std::floor(a);	//this is the integer part
				double Y = std::floor(255.0 * (a - X)); //fractional part from 0 to 255
				switch (X) {
				case 0:
					r = 255; g = Y; b = 0;
					break;
				case 1: r = 255 - Y; g = 255; b = 0; break;
				case 2: r = 0; g = 255; b = Y; break;
				case 3: r = 0; g = 255 - Y; b = 255; break;
				case 4: r = Y; g = 0; b = 255; break;
				case 5: r = 255; g = 0; b = 255; break;
				}

				//p = .8
				//a = 1-.8 = .2 / 0.2 = 1.
				//x = 1
				//y = 1. -1 = 0 * 255 = 0

				//store pixel data
				int i = 0;
				uint8_t* bytes = reinterpret_cast<uint8_t*>(&i);
				bytes[0] = r; // PFG_RGBA8_UNORM_SRGB
				bytes[1] = g;
				bytes[2] = b;
				bytes[3] = 255; // A = opaque

				std::size_t index = &f - front;
				converted[index] = i;

				});
		}
	}

	void saveToFile_colorize(const std::string& fileName, std::span<float> imageData, std::size_t w, std::size_t h, bool nrgb = true, double stripeNum = 1) {

		if (imageData.size() == 0) return;

		FreeImage_Initialise();

		//convert to NICKRGB or ROYGBIV
		std::vector<uint32_t> converted(imageData.size());

		floatSpaceConvert(imageData, converted, nrgb, 0.0, 1.0, stripeNum);
		//converted is a vector of of int32 data or RGBA, where A = 0

		uint8_t* bytes = reinterpret_cast<uint8_t*>(converted.data());
		//converted data are four byte type (int32)
		//r g b a

		int pitch = w * (32 / 8);

		FIBITMAP* convertedImage = FreeImage_ConvertFromRawBits(bytes, w, h, pitch, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);

		FreeImage_Save(FIF_BMP, convertedImage, (fileName + ".bmp").c_str(), 0);

		FreeImage_Unload(convertedImage);
		FreeImage_DeInitialise();
	}

	void readFITSimageAndConvert(const std::string& fileName) {

		fitsfile* fptr;
		int status, nfound, anynull, bitpix, naxis;
		long naxes[10], fpixel, nbuffer, npixels, ii;

		#define buffsize 1000
		float nullval, buffer[buffsize];

		status = 0;

		if (fits_open_file(&fptr, fileName.c_str(), READONLY, &status))
			throw std::exception("failed to open fits");

		char errmsg[FLEN_ERRMSG];
		fits_get_errstatus(status, errmsg);
		printf("Error %d: %s\n", status, errmsg);

		std::size_t idx = 0;
		std::vector<float> image;
		do {
			fpixel = 1;
			nullval = 0;

			fits_get_img_param(fptr, 10, &bitpix, &naxis, naxes, &status);

			if (naxis != 0) {

				std::size_t width = naxes[0], height = naxes[1];
				npixels = width * height;

				image.reserve(width * height);
				image.clear();

				while (npixels > 0) {

					nbuffer = npixels;
					if (npixels > buffsize)
						nbuffer = buffsize;

					if (fits_read_img(fptr, TFLOAT, fpixel, nbuffer, &nullval,
						buffer, &anynull, &status))

						throw std::exception("fits read");

					for (ii = 0; ii < nbuffer; ii++)
						image.push_back(buffer[ii]);

					npixels -= nbuffer;
					fpixel += nbuffer;
				}

				for (auto stripeNum : { 1,2,10,20,50,100 }) {

					saveToFile_colorize(std::format("{}_nickrgb{}_{}", fileName, idx, stripeNum), image, width, height, true, stripeNum);
					saveToFile_colorize(std::format("{}_roygbiv{}_{}", fileName, idx, stripeNum), image, width, height, false, stripeNum);
				}
			}
			fits_movrel_hdu(fptr, 1, NULL, &status);

			++idx;
		} while (status != END_OF_FILE);
		status = 0;

		if (fits_close_file(fptr, &status))
			throw std::exception("fits close");
	}
};

