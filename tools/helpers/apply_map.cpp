#include <fstream>
#include <iostream>
#include <sstream>
#include <string>


int main() {
  std::ifstream mapIn("map_array_generated.txt");
  std::stringstream mapBuf;
  mapBuf << mapIn.rdbuf();
  std::string mapStr = mapBuf.str();

  std::ifstream cppIn("latest_map.cpp"); // a clean backup we'll checkout
  std::stringstream cppBuf;
  cppBuf << cppIn.rdbuf();
  std::string cppStr = cppBuf.str();

  // 1. Insert the generated function after g_RedoStack
  std::string funcTarget = "static const int MAX_UNDO = 50;";
  size_t pos = cppStr.find(funcTarget);
  if (pos != std::string::npos) {
    cppStr.replace(pos, funcTarget.length(), funcTarget + "\n\n" + mapStr);
  }

  // 2. Make Map_Initialize call Map_LoadDefaultObjects() directly
  std::string initTarget =
      "  FILE *fp = nullptr;\n  fopen_s(&fp, \"resource/map_save.txt\", "
      "\"r\");\n  if (fp) {\n    fclose(fp);\n    "
      "Map_LoadFromFile(\"resource/map_save.txt\");\n  }";
  std::string initReplace =
      "  FILE *fp = nullptr;\n  fopen_s(&fp, \"resource/map_save.txt\", "
      "\"r\");\n  if (fp) {\n    fclose(fp);\n    "
      "Map_LoadFromFile(\"resource/map_save.txt\");\n  } else {\n    "
      "Map_LoadDefaultObjects();\n  }";
  pos = cppStr.find(initTarget);
  if (pos != std::string::npos) {
    cppStr.replace(pos, initTarget.length(), initReplace);
  }

  // 3. Fix the render state bug
  std::string bugTarget =
      "resolvedTex = obj.TextureId;\n    }\n\n    switch (obj.KindId)";
  std::string bugFix =
      "resolvedTex = obj.TextureId;\n    }\n\n    "
      "Direct3D_SetDepthEnable(true);\n\n    switch (obj.KindId)";
  pos = cppStr.find(bugTarget);
  if (pos != std::string::npos) {
    cppStr.replace(pos, bugTarget.length(), bugFix);
  }

  std::ofstream cppOut("map.cpp");
  cppOut << cppStr;

  return 0;
}
