#include <srcslicepolicy.hpp>
#include <algorithm>
#include <fstream>

int main(int argc, char** argv){
        if(argc < 2){
            std::cerr<<"Syntax: ./srcslice <srcML file name> [metric] [output file]" <<std::endl;
            return 0;
        }
        std::unordered_map<std::string, std::vector<SliceProfile>> profileMap;
        SrcSlicePolicy* cat = new SrcSlicePolicy(&profileMap);
        srcSAXController control(argv[1]);
        srcSAXEventDispatch::srcSAXEventDispatcher<> handler({cat});
        control.parse(&handler); //Start parsing
        // final module level metrics
        unsigned int moduleSize = 0;
        unsigned int sliceIdentifiers = 0;
        unsigned int finalSlices = 0;
        // maps for storing function and variable level metrics
        std::unordered_map<std::string, std::unordered_map<std::string, double>> functionMetrics;
        std::unordered_map<std::string, std::unordered_map<std::string, double>> variableMetrics;
        for(auto it : profileMap){
                for(const auto& profile : it.second){
                    // only print / do statistics for slices with declarations
                    if (profile.containsDeclaration) {
                        // only do metrics if more than 3 arguments
                        if (argc >= 3) {
                            variableMetrics[profile.variableName]["sliceSize"] =
                                    profile.definitions.size() + profile.uses.size();
                            variableMetrics[profile.variableName]["sliceIdentifiers"] =
                                    profile.dvars.size() + profile.aliases.size() + profile.cfunctions.size();
                            auto useMinMax = std::minmax_element(profile.uses.begin(), profile.uses.end());
                            auto defMinMax = std::minmax_element(profile.definitions.begin(), profile.definitions.end());
                            double maxLine = std::max(*useMinMax.second, *defMinMax.second);
                            double minLine = std::min(*useMinMax.first, *defMinMax.first);

                            variableMetrics[profile.variableName]["sliceDistance"] = maxLine - minLine;
                            moduleSize += variableMetrics[profile.variableName]["sliceSize"];
                            finalSlices++;
                            sliceIdentifiers += variableMetrics[profile.variableName]["sliceIdentifiers"];
                            // do function level metrics
                            functionMetrics[profile.function]["functionIdentifiers"] +=
                                    variableMetrics[profile.variableName]["sliceIdentifiers"];
                            functionMetrics[profile.function]["avgSliceDistance"] +=
                                    variableMetrics[profile.variableName]["sliceDistance"];
                            functionMetrics[profile.function]["sliceCount"]++;
                            functionMetrics[profile.function]["maxLine"] =
                                    std::max(maxLine, functionMetrics[profile.function]["maxLine"]);
                            functionMetrics[profile.function]["minLine"] =
                                    std::min(minLine, functionMetrics[profile.function]["minLine"]);
                            functionMetrics[profile.function]["functionSize"] +=
                                    variableMetrics[profile.variableName]["sliceSize"];

                        }
                        profile.PrintProfile();
                    }
                }
        }

        if (argc >= 3) {
            // calculate final metrics for variable slices
            for (auto& it: variableMetrics) {
                it.second["sliceCoverage"] = it.second["sliceSize"] / (double) moduleSize;
                it.second["sliceSpatial"] =  it.second["sliceDistance"] / (double) moduleSize;
            }

            // calculate coverage then fix all the averages for each function
            for (auto& it : functionMetrics) {
                it.second["functionCoverage"] = it.second["functionSize"] / (double) moduleSize;
                it.second["avgSliceDistance"] /= it.second["sliceCount"];
                it.second["avgSliceSize"] = it.second["functionSize"] / it.second["sliceCount"];
                it.second["avgSliceIdentifiers"] = it.second["functionIdentifiers"] / it.second["sliceCount"];
                it.second["functionDistance"] = it.second["maxLine"] - it.second["minLine"];
                it.second["functionSpatial"] = it.second["functionDistance"] / (double) moduleSize;
            }
            // print out results
            std::cout << "Source Metrics for module " << std::endl;
            std::cout << "--- VARIABLE LEVEL ---" << std::endl;
            for(auto it : variableMetrics) {
                std::cout << "Variable name:" << it.first << std::endl;
                std::cout << "Slice size: " << it.second["sliceSize"] << std::endl;
                std::cout << "Slice identifiers: " << it.second["sliceIdentifiers"] << std::endl;
                std::cout << "Slice distance: " << it.second["sliceDistance"] << std::endl;
                std::cout << "Slice coverage: " << it.second["sliceCoverage"] << std::endl;
                std::cout << "Slice spatial: " << it.second["sliceSpatial"] << std::endl;
            }
            std::cout << "--- FUNCTION LEVEL ---" << std::endl;
            for(auto it : functionMetrics) {
                std::cout << "Function name:" << it.first << std::endl;
                std::cout << "Function slice count: " << it.second["sliceCount"] << std::endl;
                std::cout << "Function average slice distance: " << it.second["avgSliceDistance"] << std::endl;
                std::cout << "Function average slice size: " << it.second["avgSliceSize"] << std::endl;
                std::cout << "Function identifiers: " << it.second["functionIdentifiers"] << std::endl;
                std::cout << "Function size: " << it.second["functionSize"] << std::endl;
                std::cout << "Function distance: " << it.second["functionDistance"] << std::endl;
                std::cout << "Function spatial: " << it.second["functionSpatial"] << std::endl;
                std::cout << "Function coverage: " << it.second["functionCoverage"] << std::endl;
            }


            double avgSliceIdentifiers = (double) sliceIdentifiers / finalSlices;
            double avgSliceSize = (double) moduleSize / finalSlices;
            std::cout << "--- MODULE LEVEL ---" << std::endl;
            std::cout << "Total SLOC: " << moduleSize << std::endl;
            std::cout << "Slice count: " << profileMap.size() << std::endl;
            std::cout << "Average slice size: " << avgSliceSize << std::endl;
            std::cout << "Average slice identifiers: " << avgSliceIdentifiers << std::endl;

            // Print metrics to csv
            if (argc >= 4) {
                std::ofstream outStream;
                outStream.open(std::string(argv[3]) + "_module");
                outStream << "sloc,sliceCount,avgSliceSize,avgSliceIdentifiers" << std::endl;
                outStream << moduleSize << "," << profileMap.size() << "," << moduleSize << "," << sliceIdentifiers <<
                          std::endl;
                outStream.close();
                outStream.open(std::string(argv[3]) + "_function");
                for (auto it : functionMetrics) {
                    outStream << "function,sliceCount,avgSliceDistance,avgSliceSize,avgSliceIdentifiers,"
                              << "functionIdentifiers,functionSize,functionDistance,functionSpatial,coverage"
                              << std::endl;
                    outStream << it.first << "," << it.second["sliceCount"] << ","
                              << it.second["avgSliceDistance"] << it.second["avgSliceSize"] << it.second["avgSliceIdentifiers"]
                              << it.second["functionIdentifiers"] << it.second["functionSize"] << it.second["functionDistance"]
                              << it.second["functionSpatial"] << it.second["functionCoverage"];
                }
                outStream.close();
                outStream.open(std::string(argv[3]) + "_variable");
                for (auto it : variableMetrics) {
                    outStream << "variable,sliceSize,sliceIdentifiers,sliceDistance,sliceCoverage,sliceSpatial"
                              << std::endl;
                    outStream << it.first << "," << it.second["sliceSize"] << "," << it.second["sliceIdentifiers"] << ","
                              << it.second["sliceDistance"] << "," << it.second["sliceCoverage"] << "," <<
                              it.second["sliceSpatial"] << std::endl;
                }
                outStream.close();
            }

        }
}