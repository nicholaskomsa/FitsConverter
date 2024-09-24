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

	enum class ColorizeMode {
		NICKRGB,
		ROYGBIV,
		GREYSCALE
	};

	void floatSpaceConvert(std::span<const float> data, std::span<uint32_t> converted, ColorizeMode colorMode = ColorizeMode::NICKRGB, double vMin = 0.0, double vMax = 1.0, double stripeNum = 1) {

		auto getViewWindow = [&](double startPercent = 0.0, double endPercent = 1.0) ->std::tuple<double, double, double> {

			auto minmax = std::minmax_element(data.begin(), data.end()); 
			auto min = *minmax.first, max = *minmax.second;

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

			double percent = 1.0;

			f -= viewMin;

			if (f < viewDistance) {
				f -= stripeDistance * std::floor(f / stripeDistance);

				percent = f / stripeDistance;
			}

			//percent is between 0 and 1
			return percent;
		};

		auto clearAlpha = [&](uint32_t& i) {
			reinterpret_cast<uint8_t*>(&i)[3] = 0;
		};
		auto setOpaque = [&](uint32_t& i) {
			reinterpret_cast<uint8_t*>(&i)[3] = 255;
		};		
		auto rgb = [&](std::uint8_t r, std::uint8_t g, std::uint8_t b) {

			std::uint32_t rgba = 0;
			std::uint8_t* bytes = reinterpret_cast<std::uint8_t*>(&rgba);
			bytes[0] = r; //PFG_BGRA8_UNORM_SRGB
			bytes[1] = g;
			bytes[2] = b;

			return rgba;
		};

		auto forEachPixel = [&](auto&& colorize) {

			//we are running par on images/stripeNum instead of here
			std::transform(std::execution::seq, data.begin(), data.end(), converted.begin(), [&](auto& f) {

				auto percent = convertToGreyScale(f);

				std::uint32_t rgba = colorize(percent);

				//we want these pixels to be defined as completly non-transparent
				setOpaque(rgba);
				
				return rgba;

				});
			};

		switch (colorMode) {
		case ColorizeMode::NICKRGB:{

			uint32_t intMax = std::numeric_limits<uint32_t>::max();//max will use alpha
			//we want to perfectly fit into rgb space, so
			//get rid of alpha channel, rgb remains, a = 0 
			clearAlpha(intMax);

			forEachPixel([&](auto percent) {

				//this is the nickrgb transform
				//project onto integer	// int RGB

				return intMax * percent;

				});
			}break;

		case ColorizeMode::ROYGBIV: {
			
			forEachPixel([&](auto percent) {

				uint8_t r = 0, g = 0, b = 0;

				/*plot short rainbow RGB*/
				float a = (1.0 - percent) / 0.20;	//invert and group
				int X = std::floor(a);	//this is the integer part
				float Y = std::floor(255.0 * (a - X)); //fractional part from 0 to 255
				switch (X) {
				case 0: r = 255; g = Y; b = 0; break;
				case 1: r = 255 - Y; g = 255; b = 0; break;
				case 2: r = 0; g = 255; b = Y; break;
				case 3: r = 0; g = 255 - Y; b = 255; break;
				case 4: r = Y; g = 0; b = 255; break;
				case 5: r = 255; g = 0; b = 255; break;
				}

				return rgb(r,g,b);

				});

			} break;

		case ColorizeMode::GREYSCALE: {

			forEachPixel([&](auto percent) {

				uint8_t grey = 255 * percent;

				return rgb(grey, grey, grey);

				});
			} break;
		}
	}

	void saveToBmpFile(std::string fileName, std::span<uint32_t> image, std::size_t w, std::size_t h) {

		if (image.size() == 0) return;

		uint8_t* bytes = reinterpret_cast<uint8_t*>(image.data());
		//converted data are four byte type (int32)
		//r g b a

		int pitch = w * (32 / 8);

		FIBITMAP* convertedImage = FreeImage_ConvertFromRawBits(bytes, w, h, pitch, 32, FI_RGBA_RED_MASK, FI_RGBA_GREEN_MASK, FI_RGBA_BLUE_MASK);

		FreeImage_Save(FIF_BMP, convertedImage, fileName.c_str(), 0);

		FreeImage_Unload(convertedImage);
	}

	void readFITSimagesAndColorize(const std::string& fileName) {

		auto writeColorizedImages = [&](auto idx, auto& image, auto width, auto height) {

			auto fileNameWithIdx = std::format("{}_{}", fileName, idx);

			auto stripes = { 1,2,10,20,50,100 };
			auto colorizeModes = { ColorizeMode::GREYSCALE, ColorizeMode::ROYGBIV, ColorizeMode::NICKRGB };

			auto colorizeModeStr = [&](auto colorizeMode) {
				switch (colorizeMode) {
				case ColorizeMode::NICKRGB: return "nickrgb";
				case ColorizeMode::ROYGBIV: return "roygbiv";
				case ColorizeMode::GREYSCALE: return "greyscale";
				}
				return "unknown";
				};

			FreeImage_Initialise();

			std::for_each(std::execution::par, stripes.begin(), stripes.end(), [&](int stripeNum) {

				std::vector<uint32_t> converted(image.size());

				for (auto colorizeMode : colorizeModes) {

					floatSpaceConvert(image, converted, colorizeMode, 0.0f, 1.0f, stripeNum);

					auto completeFileNameWithColorMode = std::format("{}_{}_{}.bmp", fileName, colorizeModeStr(colorizeMode), stripeNum);
					saveToBmpFile(completeFileNameWithColorMode, converted, width, height);
				}

				});

			FreeImage_DeInitialise();

			};

		auto readFitsImages = [&]() {

			fitsfile* fptr;
			int status, nfound, anynull, bitpix, naxis;
			long naxes[10], fpixel, nbuffer, npixels, ii;

			constexpr long buffsize = 1000;
			float nullval, buffer[buffsize];

			status = 0;

			if (fits_open_file(&fptr, fileName.c_str(), READONLY, &status))
				throw std::exception("failed to open fits");

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

					writeColorizedImages(idx, image, width, height);

				}
				fits_movrel_hdu(fptr, 1, NULL, &status);

				++idx;
			} while (status != END_OF_FILE);

			status = 0;

			if (fits_close_file(fptr, &status))
				throw std::exception("fits close");
		};


		readFitsImages();

	}
};

