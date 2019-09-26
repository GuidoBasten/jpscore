#include "Trajectories.h"

#include <tinyxml.h>

#include "geometry/SubRoom.h"
#include "mpi/LCGrid.h"
#include "pedestrian/Pedestrian.h"

static fs::path getSourceFileName(const fs::path& projectFile)
{
     fs::path ret{};

     TiXmlDocument doc(projectFile.string());
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \tGetSourceFileName could not parse the project file");
          return ret;
     }
     TiXmlNode* xRootNode = doc.RootElement()->FirstChild("agents");
     if (!xRootNode) {
          Log->Write("ERROR:\tGetSourceFileName could not load persons attributes");
          return ret;
     }

     TiXmlNode* xSources = xRootNode->FirstChild("agents_sources");
     if (xSources) {
          TiXmlNode* xFileNode = xSources->FirstChild("file");
          //------- parse sources from external file
          if(xFileNode)
          {
               ret = xFileNode->FirstChild()->ValueStr();
          }
          return ret;
     }
     return ret;
}

static fs::path getEventFileName(const fs::path& projectFile)
{
     fs::path ret{};

     TiXmlDocument doc(projectFile.string());
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \tGetEventFileName could not parse the project file");
          return ret;
     }
     TiXmlNode* xMainNode = doc.RootElement();
     std::string eventfile = "";
     if (xMainNode->FirstChild("events_file")) {
          ret = xMainNode->FirstChild("events_file")->FirstChild()->ValueStr();
          Log->Write("INFO: \tevents <" + ret.string() + ">");
     } else {
          Log->Write("INFO: \tNo events found");
          return ret;
     }
     return ret;
}
// <train_constraints>
//   <train_time_table>ttt.xml</train_time_table>
//   <train_types>train_types.xml</train_types>
// </train_constraints>


static fs::path getTrainTimeTableFileName(const fs::path& projectFile)
{
     fs::path ret{};

     TiXmlDocument doc(projectFile.string());
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \tGetTrainTimeTable could not parse the project file");
          return ret;
     }
     TiXmlNode* xMainNode = doc.RootElement();
     std::string tttfile = "";
     if (xMainNode->FirstChild("train_constraints")) {
          TiXmlNode * xFileNode = xMainNode->FirstChild("train_constraints")->FirstChild("train_time_table");

          if(xFileNode)
               ret = xFileNode->FirstChild()->ValueStr();
          Log->Write("INFO: \ttrain_time_table <" + ret.string() + ">");
     } else {
          Log->Write("INFO: \tNo events no ttt file found");
          return ret;
     }
     return ret;
}

static fs::path getTrainTypeFileName(const fs::path& projectFile)
{
     fs::path ret{};

     TiXmlDocument doc(projectFile.string());
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \tGetTrainType could not parse the project file");
          return ret;
     }
     TiXmlNode* xMainNode = doc.RootElement();
     std::string tttfile = "";
     if (xMainNode->FirstChild("train_constraints")) {
          auto xFileNode = xMainNode->FirstChild("train_constraints")->FirstChild("train_types");
          if(xFileNode)
               ret = xFileNode->FirstChild()->ValueStr();
          Log->Write("INFO: \ttrain_types <" + ret.string() + ">");
     } else {
          Log->Write("INFO: \tNo events no train types file found");
          return ret;
     }
     return ret;
}

static fs::path getGoalFileName(const fs::path& projectFile)
{
     fs::path ret{};

     TiXmlDocument doc(projectFile.string());
     if (!doc.LoadFile()) {
          Log->Write("ERROR: \t%s", doc.ErrorDesc());
          Log->Write("ERROR: \tGetSourceFileName could not parse the project file");
          return ret;
     }
     TiXmlNode* xRootNode = doc.RootElement();
     if (!xRootNode->FirstChild("routing")) {
          return ret;
     }
     //load goals and routes
     TiXmlNode* xGoalsNode = xRootNode->FirstChild("routing")->FirstChild("goals");
     TiXmlNode* xGoalsNodeFile = xGoalsNode->FirstChild("file");
     if(xGoalsNodeFile)
     {
          ret = xGoalsNodeFile->FirstChild()->ValueStr();
          Log->Write("INFO:\tGoal file <%s> will be parsed", ret.string().c_str());
     }
     return ret;
}

/**
 * TXT format implementation
 */
void TrajectoriesTXT::WriteHeader(long nPeds, double fps, Building* building, int seed, int count)
{
     const fs::path projRoot(building->GetProjectRootDir());

     (void) seed; (void) nPeds;
     char tmp[500] = "";
     sprintf(tmp, "#description: jpscore (%s)", JPSCORE_VERSION);
     Write(tmp);
     sprintf(tmp, "#count: %d", count);
     Write(tmp);
     sprintf(tmp, "#framerate: %0.2f",fps);
     Write(tmp);
     const fs::path tmpGeo = projRoot / building->GetGeometryFilename();
     sprintf(tmp,"#geometry: %s",  tmpGeo.string().c_str());
     Write(tmp);

     if(const fs::path sourceFileName = getSourceFileName(building->GetProjectFilename());
             !sourceFileName.empty())
     {
          const fs::path tmpSource = projRoot / sourceFileName;
          sprintf(tmp,"#sources: %s", tmpSource.string().c_str());
          Write(tmp);
     }

     if(const fs::path goalFileName = getGoalFileName(building->GetProjectFilename());
             !goalFileName.empty())
     {
          const fs::path tmpGoal = projRoot / goalFileName;
          sprintf(tmp,"#goals: %s", tmpGoal.string().c_str());
          Write(tmp);
     }

     if(const fs::path eventFileName = getEventFileName(building->GetProjectFilename());
             !eventFileName.empty())
     {
          const fs::path tmpEvent = projRoot / eventFileName;
          sprintf(tmp,"#events: %s", tmpEvent.string().c_str());
          Write(tmp);
     }

     if(const fs::path  trainTimeTableFileName = getTrainTimeTableFileName(building->GetProjectFilename());
             !trainTimeTableFileName.empty())
     {
          const fs::path tmpTTT = projRoot / trainTimeTableFileName;
          sprintf(tmp,"#trainTimeTable: %s", tmpTTT.string().c_str());
          Write(tmp);
     }

     if(const fs::path  trainTypeFileName = getTrainTypeFileName(building->GetProjectFilename());
             !trainTypeFileName.empty())
     {
          const fs::path tmpTT = projRoot / trainTypeFileName;
          sprintf(tmp,"#trainType: %s", tmpTT.string().c_str());
          Write(tmp);
     }
     Write("#ID: the agent ID");
     Write("#FR: the current frame");
     Write("#X,Y,Z: the agents coordinates (in metres)");
     Write("#A, B: semi-axes of the ellipse");
     Write("#ANGLE: orientation of the ellipse");
     Write("#COLOR: color of the ellipse");

     Write("\n");
     //Write("#ID\tFR\tX\tY\tZ");// @todo: maybe use two different formats
     Write("#ID\tFR\tX\tY\tZ\tA\tB\tANGLE\tCOLOR");// a b angle color
}

void TrajectoriesTXT::WriteGeometry(Building* building)
{
     (void) building;
}

void TrajectoriesTXT::WriteFrame(int frameNr, Building* building)
{
     char tmp[CLENGTH] = "";
     const std::vector< Pedestrian* >& allPeds = building->GetAllPedestrians();
     for(unsigned int p=0;p<allPeds.size();p++){
          Pedestrian* ped = allPeds[p];
          double x = ped->GetPos()._x;
          double y = ped->GetPos()._y;
          double z = ped->GetElevation();
          int color=ped->GetColor();
          double a = ped->GetLargerAxis();
          double b = ped->GetSmallerAxis();
          double phi = atan2(ped->GetEllipse().GetSinPhi(), ped->GetEllipse().GetCosPhi());
          double RAD2DEG = 180.0 / M_PI;
          // @todo: maybe two different formats
          //sprintf(tmp, "%d\t%d\t%0.2f\t%0.2f\t%0.2f", ped->GetID(), frameNr, x, y, z);
          sprintf(tmp, "%d\t%d\t%0.2f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\t%0.2f\t%d", ped->GetID(), frameNr, x, y, z, a, b, phi * RAD2DEG, color);
          Write(tmp);
     }
}

void TrajectoriesXML::WriteHeader(long nPeds, double fps, Building* building, int seed, int /*count*/)
{
     building->GetCaption();
     std::string tmp;
     tmp = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n" "<trajectories>\n";
     char agents[CLENGTH] = "";
     sprintf(agents, "\t<header version = \"0.5.1\">\n");
     tmp.append(agents);
     sprintf(agents, "\t\t<agents>%ld</agents>\n", nPeds);
     tmp.append(agents);
     sprintf(agents, "\t\t<seed>%d</seed>\n", seed);
     tmp.append(agents);
     sprintf(agents, "\t\t<frameRate>%0.2f</frameRate>\n", fps);
     tmp.append(agents);
     //tmp.append("\t\t<!-- Frame count HACK\n");
     //tmp.append("replace me\n");
     //tmp.append("\t\tFrame count HACK -->\n");
     //tmp.append("<frameCount>xxxxxxx</frameCount>\n");
     tmp.append("\t</header>\n");
     _outputHandler->Write(tmp);
}

void TrajectoriesXML::WriteSources(const std::vector<std::shared_ptr<AgentsSource>>& sources)
{
     std::string tmp("");

     for (const auto& src: sources) {
          auto BB =  src->GetBoundaries();
          tmp += "<source  id=\"" + std::to_string(src->GetId()) +
                  "\"  caption=\"" + src->GetCaption() + "\"" +
                  "  x_min=\"" + std::to_string(BB[0]) + "\"" +
                  "  x_max=\"" + std::to_string(BB[1]) + "\"" +
                  "  y_min=\"" + std::to_string(BB[2]) + "\"" +
                  "  y_max=\"" + std::to_string(BB[3]) + "\" />\n";
     }
     _outputHandler->Write(tmp);
}

void TrajectoriesXML::WriteGeometry(Building* building)
{
     // just put a link to the geometry file
     std::string embed_geometry;
     embed_geometry.append("\t<geometry>\n");
     char file_location[CLENGTH] = "";
     sprintf(file_location, "\t<file location= \"%s\"/>\n", building->GetGeometryFilename().string().c_str());
     embed_geometry.append(file_location);
     //embed_geometry.append("\t</geometry>\n");

     for (auto hline: building->GetAllHlines())
     {
          embed_geometry.append(hline.second->GetDescription());
     }

     for (auto goal: building->GetAllGoals()) {
          embed_geometry.append(goal.second->Write());
     }

     //write the grid
     //embed_geometry.append(building->GetGrid()->ToXML());

     embed_geometry.append("\t</geometry>\n");
     _outputHandler->Write(embed_geometry);
     //write sources
     // if(building->G )
     //
     _outputHandler->Write("\t<AttributeDescription>");
     _outputHandler->Write("\t\t<property tag=\"x\" description=\"xPosition\"/>");
     _outputHandler->Write("\t\t<property tag=\"y\" description=\"yPosition\"/>");
     _outputHandler->Write("\t\t<property tag=\"z\" description=\"zPosition\"/>");
     _outputHandler->Write("\t\t<property tag=\"rA\" description=\"radiusA\"/>");
     _outputHandler->Write("\t\t<property tag=\"rB\" description=\"radiusB\"/>");
     _outputHandler->Write("\t\t<property tag=\"eC\" description=\"ellipseColor\"/>");
     _outputHandler->Write("\t\t<property tag=\"eO\" description=\"ellipseOrientation\"/>");
     _outputHandler->Write("\t</AttributeDescription>\n");
}

void TrajectoriesXML::WriteFrame(int frameNr, Building* building)
{
     std::string data;
     char tmp[CLENGTH] = "";
     double RAD2DEG = 180.0 / M_PI;

     sprintf(tmp, "<frame ID=\"%d\">\n", frameNr);
     data.append(tmp);

     const std::vector< Pedestrian* >& allPeds = building->GetAllPedestrians();
     for(unsigned int p=0;p<allPeds.size();p++)
     {
          Pedestrian* ped = allPeds[p];
          Room* r = building->GetRoom(ped->GetRoomID());
          std::string caption = r->GetCaption();
          char s[CLENGTH] = "";
          int color=ped->GetColor();
          double a = ped->GetLargerAxis();
          double b = ped->GetSmallerAxis();
          double phi = atan2(ped->GetEllipse().GetSinPhi(), ped->GetEllipse().GetCosPhi());
          sprintf(s, "<agent ID=\"%d\"\t"
                     "x=\"%.6f\"\ty=\"%.6f\"\t"
                     "z=\"%.6f\"\t"
                     "rA=\"%.2f\"\trB=\"%.2f\"\t"
                     "eO=\"%.2f\" eC=\"%d\"/>\n",
                  ped->GetID(), (ped->GetPos()._x) * FAKTOR,
                  (ped->GetPos()._y) * FAKTOR,(ped->GetElevation()) * FAKTOR ,a * FAKTOR, b * FAKTOR,
                  phi * RAD2DEG, color);
          data.append(s);
     }
     data.append("</frame>\n");
     _outputHandler->Write(data);
}

void TrajectoriesXML::WriteFooter()
{
     _outputHandler->Write("</trajectories>\n");
}