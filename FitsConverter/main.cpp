
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimageAndConvert(".//spaceimages//m42proc.fits");

    std::cout << "\nprogram finished\n";
}
