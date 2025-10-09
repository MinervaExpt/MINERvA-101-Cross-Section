//File: GetPlaylist.cpp
//Brief: A function that looks at an AnaTuple file and figures out the playlist name for the first event.
//Author: Andrew Olivier aolivier@ur.rochester.edu

//app includes
#include "util/GetPlaylist.h"

//PlotUtils includes
#include "PlotUtils/TreeWrapper.h"

//c++ includes
#include <iostream>

namespace
{
  //A mapping from first run number to name of a playlist.
  //I combined MC and data runs into the same map.
  //TODO: If I ever need more run period metadata, seriously look into a database.
  //TODO: I'm missing LE
  //TODO: Some kind of check on whether a run is in the range of a playlist.
  std::map<int, std::string> runNumberToPlaylist = {
                                                     //MC from https://cdcvs.fnal.gov/redmine/projects/minerva-sw/wiki/Inextinguishable_Monte_Carlo_Production_Run_Numbers
                                                     //TODO: Deepika says I should get run ranges from here instead: https://cdcvs.fnal.gov/redmine/projects/minerva-sw/wiki/Inextinguishable_Production
                                                     //I've already commented out me1Y and me1Z, so hopefully that's the only major change.
                                                     {110000, "minervame1A"}, //tracker
                                                     {114000, "minervame1A"}, //nuke
                                                     {111000, "minervame1B"}, //tracker
                                                     {115000, "minervame1B"}, //nuke
                                                     {111030, "minervame1C"}, //tracker
                                                     {115030, "minervame1C"}, //nuke
                                                     {111100, "minervame1D"}, //tracker
                                                     {115100, "minervame1D"}, //nuke
                                                     {111325, "minervame1E"}, //tracker
                                                     {115325, "minervame1E"}, //nuke
                                                     {111490, "minervame1F"}, //tracker
                                                     {115490, "minervame1F"}, //nuke
                                                     {110150, "minervame1G"}, //tracker
                                                     {114150, "minervame1G"}, //nuke
                                                     {113000, "minervame1L"}, //tracker
                                                     {117000, "minervame1L"}, //nuke
                                                     {113020, "minervame1M"}, //tracker
                                                     {117020, "minervame1M"}, //nuke
                                                     {113270, "minervame1N"}, //tracker
                                                     {117270, "minervame1N"}, //nuke
                                                     {113375, "minervame1O"}, //tracker
                                                     {117375, "minervame1O"}, //nuke
                                                     {112000, "minervame1P"}, //tracker
                                                     {116000, "minervame1P"}, //nuke
                                                     {123000, "minervame5A"}, //tracker
                                                     {127000, "minervame5A"}, //nuke
                                                     {122000, "minervame6A"}, //tracker
                                                     {126000, "minervame6A"}, //nuke
                                                     {122240, "minervame6B"}, //tracker
                                                     {126240, "minervame6B"}, //nuke
                                                     {122390, "minervame6C"}, //tracker
                                                     {126390, "minervame6C"}, //nuke
                                                     {122565, "minervame6D"}, //tracker
                                                     {126565, "minervame6D"}, //nuke
                                                     {122730, "minervame6E"}, //tracker
                                                     {126730, "minervame6E"}, //nuke
                                                     {122880, "minervame6F"}, //tracker
                                                     {126880, "minervame6F"}, //nuke
                                                     {123100, "minervame6G"}, //tracker
                                                     {127100, "minervame6G"}, //nuke
                                                     {123250, "minervame6H"}, //tracker
                                                     {126240, "minervame6H"}, //nuke
                                                     {123425, "minervame6I"}, //tracker
                                                     {127425, "minervame6I"}, //nuke
                                                     {123565, "minervame6J"}, //tracker
                                                     {127100, "minervame6J"}, //nuke
                                                     {131000, "minervame3A"}, //tracker
                                                     {135000, "minervame3A"}, //nuke
                                                     /*{113020, "minervame1Y"}, //tracker
                                                     {117020, "minervame1Y"}, //nuke
                                                     {113055, "minervame1Z"}, //tracker
                                                     {117055, "minervame1Z"}, //nuke*/
                                                     //Data from https://cdcvs.fnal.gov/redmine/projects/minerva-sw/wiki/Data_Run_Periods
                                                     {6038, "minervame1A"},
                                                     {7000, "minervame2"},
                                                     {9000, "minervame3"},
                                                     {16854, "minervame3A"},
                                                     {16838, "minervame4A"},
                                                     {16842, "minervame4B"},
                                                     {16845, "minervame4C"},
                                                     {16848, "minervame4D"},
                                                     {16849, "minervame4E"},
                                                     {10068, "minervame1B"},
                                                     {10129, "minervame1C"},
                                                     {10255, "minervame1D"},
                                                     {16003, "minervame1E"},
                                                     {16523, "minervame1F"},
                                                     {18668, "minervame1G"},
                                                     {19167, "minervame1H"},
                                                     {19168, "minervame1L"},
                                                     {19186, "minervame1M"},
                                                     {19500, "minervame5A"},
                                                     {22000, "minervame6A"},
                                                     {22398, "minervame6B"},
                                                     {20328, "minervame1N"},
                                                     {20619, "minervame1O"},
                                                     {20800, "minervame1P"},
                                                     {25001, "minervame6C"},
                                                     {25174, "minervame6D"},
                                                     {25283, "minervame6E"},
                                                     {25374, "minervame6F"},
                                                     {25612, "minervame6G"},
                                                     {26000, "minervame6H"},
                                                     {26201, "minervame6I"},
                                                     {26277, "minervame6J"},
                                                     {20300, "minervame7"}
                                                   };
}

namespace util
{
  std::string GetPlaylist(PlotUtils::TreeWrapper& tree, const bool isMC)
  {
    const int run = tree.GetValue((isMC?"mc":"ev") + std::string("_run"), 0);

    std::string playlist = "NoSuchPlaylist";
    const auto found = ::runNumberToPlaylist.upper_bound(run);
    if(found != ::runNumberToPlaylist.begin()) playlist = std::prev(found)->second;

    std::cout << "Inferring playlist " << playlist << " from run number " << run << ".\n";

    return playlist;
  }
}
