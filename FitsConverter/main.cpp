
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimagesAndColorize(".//spaceimages//aia.lev1.335A_2023-03-30T12_00_00.63Z.image_lev1.fits");

    std::cout << "\nprogram finished\n";
}
