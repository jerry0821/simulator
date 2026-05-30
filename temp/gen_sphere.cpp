#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

const double PI = 3.14159265358979323846;

int main() {
    std::ofstream f("c:/Users/a3877/OneDrive/桌面/就職活動/simulator/resource/model/sphere.obj");
    int stacks = 16;
    int slices = 32;
    double radius = 1.0;
    
    f << "o Sphere\n";
    for(int i = 0; i <= stacks; ++i) {
        double v = (double)i / stacks;
        double phi = v * PI;
        for(int j = 0; j <= slices; ++j) {
            double u = (double)j / slices;
            double theta = u * 2.0 * PI;
            double x = cos(theta) * sin(phi);
            double y = cos(phi);
            double z = sin(theta) * sin(phi);
            f << "v " << x * radius << " " << y * radius << " " << z * radius << "\n";
            f << "vn " << x << " " << y << " " << z << "\n";
            f << "vt " << u << " " << 1.0 - v << "\n";
        }
    }
    for(int i = 0; i < stacks; ++i) {
        for(int j = 0; j < slices; ++j) {
            int p1 = i * (slices + 1) + j + 1;
            int p2 = p1 + 1;
            int p3 = p1 + (slices + 1);
            int p4 = p3 + 1;
            f << "f " << p1 << "/" << p1 << "/" << p1 << " " 
              << p2 << "/" << p2 << "/" << p2 << " "
              << p4 << "/" << p4 << "/" << p4 << " "
              << p3 << "/" << p3 << "/" << p3 << "\n";
        }
    }
    return 0;
}
