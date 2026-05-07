#include <fstream>
#include <iostream>
#include <sstream>
#include <string>


int main() {
  std::ifstream in("resource/map_save.txt");
  std::ofstream out("map_array_generated.txt");

  std::string line;
  std::getline(in, line); // header

  out << "static void Map_LoadDefaultObjects() {\n";
  out << "  g_MapObjects.reserve(1000);\n";

  while (std::getline(in, line)) {
    if (line.empty())
      continue;
    std::istringstream iss(line);
    int kindId;
    float px, py, pz, rx, ry, rz, sx, sy, sz, minx, miny, minz;
    std::string tpath;

    if (!(iss >> kindId >> px >> py >> pz >> rx >> ry >> rz >> sx >> sy >> sz >>
          minx >> miny >> minz)) {
      continue;
    }
    iss >> tpath;

    std::string kindStr = "BLOCK";
    if (kindId == 0)
      kindStr = "FIELD";
    else if (kindId == 1)
      kindStr = "BLOCK";
    else if (kindId == 2)
      kindStr = "SPHERE";
    else if (kindId == 3)
      kindStr = "CYLINDER";
    else if (kindId == 4)
      kindStr = "GRASS";
    else if (kindId == 5)
      kindStr = "SLIME";
    else if (kindId == 6)
      kindStr = "ROCK";

    std::string texStr = "\"\"";
    if (tpath != "" && tpath != "-") {
      texStr = "\"" + tpath + "\"";
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
             "  g_MapObjects.push_back({(int)%s, {%.2ff, %.2ff, %.2ff}, "
             "{{%.2ff, %.2ff, %.2ff}, {%.2ff, %.2ff, %.2ff}}, {%.2ff, %.2ff, "
             "%.2ff}, {%.2ff, %.2ff, %.2ff}, %s, -1});\n",
             kindStr.c_str(), px, py, pz, minx, miny, minz, minx + sx,
             miny + sy, minz + sz, rx, ry, rz, sx, sy, sz, texStr.c_str());
    out << buf;
  }
  out << "}\n";

  return 0;
}
