
#include <iostream>

#include "FitsConverter.h"

int main(){

    FitsConverter::readFITSimagesAndColorize(".//spaceimages//m42_Grayscale.fit");

    std::cout << "\nprogram finished\n";
}
