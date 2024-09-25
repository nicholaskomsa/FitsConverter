
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimagesAndColorize(".//spaceimages//m101_ThePinwheelGalaxy.fits");

    std::cout << "\nprogram finished\n";
}
