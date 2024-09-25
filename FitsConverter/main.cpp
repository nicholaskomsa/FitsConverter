
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimagesAndColorize(".//spaceimages//ngc2174_MonkeyHeadNebula.fits");

    std::cout << "\nprogram finished\n";
}
