
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimagesAndColorize(".//spaceimages//conenebula.fits");

    std::cout << "\nprogram finished\n";
}
