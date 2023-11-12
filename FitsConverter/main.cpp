
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimageAndConvert(".//spaceimages//AIA20230426_162100_0304.fits");

    std::cout << "\nprogram finished\n";
}
