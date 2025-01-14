/**
 * \section License
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 **/
//
// Created by laemmel on 30.03.16.
//
#include "GeoFileParser.h"

#include "general/Filesystem.h"
#include "general/Format.h"
#include "general/Logger.h"
#include "geometry/SubRoom.h"
#include "geometry/WaitingArea.h"
#include "geometry/Wall.h"

#include <tinyxml.h>

GeoFileParser::GeoFileParser(Configuration * configuration) : _configuration(configuration) {}

void GeoFileParser::LoadBuilding(Building * building)
{
    //todo: what happens if any of these  methods failed (return false)? throw exception ?
    if(!LoadGeometry(building)) {
        Logging::Error("Could not load the geometry!");
        exit(EXIT_FAILURE);
    }

    if(!LoadRoutingInfo(building)) {
        Logging::Error("Could not load extra routing information!");
        exit(EXIT_FAILURE);
    }
    if(!LoadTrafficInfo(building)) {
        Logging::Error("Could not load extra traffic information!");
        exit(EXIT_FAILURE);
    }
    if(!LoadTrainInfo(building)) {
        Logging::Error("Could not load train information!");
        exit(EXIT_FAILURE);
    }
}

bool GeoFileParser::LoadGeometry(Building * building)
{
    const fs::path & rootDir(_configuration->GetProjectRootDir());

    const fs::path geoFilenameWithPath = rootDir / _configuration->GetGeometryFile();
    Logging::Info(fmt::format(check_fmt("LoadGeometry: file: {}"), geoFilenameWithPath.string()));

    TiXmlDocument docGeo(geoFilenameWithPath.string());
    if(!docGeo.LoadFile()) {
        Logging::Error("Could not parse the geometry file.");
        Logging::Error(fmt::format(check_fmt("{}"), docGeo.ErrorDesc()));
        return false;
    }

    TiXmlElement * xRootNode = docGeo.RootElement();
    if(!xRootNode) {
        Logging::Error("Root element does not exist");
        return false;
    }

    if(xRootNode->ValueStr() != "geometry") {
        Logging::Error("Root element value is not 'geometry'.");
        return false;
    }
    if(xRootNode->Attribute("unit"))
        if(std::string(xRootNode->Attribute("unit")) != "m") {
            Logging::Error("Only the unit m (meters) is supported.");
            Logging::Error(
                fmt::format(check_fmt("You supplied [{}]"), xRootNode->Attribute("unit")));
            return false;
        }

    double version = xmltof(xRootNode->Attribute("version"), -1);

    if(version < std::stod(JPS_OLD_VERSION)) {
        Logging::Error("Wrong geometry version!");
        Logging::Error(fmt::format(check_fmt("Only version >= {:s} supported."), JPS_OLD_VERSION));
        Logging::Error(fmt::format(
            check_fmt("Please update the version of your geometry file to {:s}"), JPS_OLD_VERSION));
        return false;
    }

    building->SetCaption(xmltoa(xRootNode->Attribute("caption"), "virtual building"));

    //processing the rooms node
    TiXmlNode * xRoomsNode = xRootNode->FirstChild("rooms");
    if(!xRoomsNode) {
        Logging::Error("The geometry should have at least one room and one subroom");
        return false;
    }

    for(TiXmlElement * xRoom = xRoomsNode->FirstChildElement("room"); xRoom;
        xRoom                = xRoom->NextSiblingElement("room")) {
        Room * room = new Room();
        //make_unique<Song>

        std::string room_id = xmltoa(xRoom->Attribute("id"), "-1");
        room->SetID(xmltoi(room_id.c_str(), -1));

        std::string caption = "room " + room_id;
        room->SetCaption(xmltoa(xRoom->Attribute("caption"), caption.c_str()));

        double position = xmltof(xRoom->Attribute("zpos"), 0.0);

        //if(position>6.0) position+=50;
        room->SetZPos(position);

        //parsing the subrooms
        //processing the rooms node
        //TiXmlNode*  xSubroomsNode = xRoom->FirstChild("subroom");

        for(TiXmlElement * xSubRoom = xRoom->FirstChildElement("subroom"); xSubRoom;
            xSubRoom                = xSubRoom->NextSiblingElement("subroom")) {
            //               std::string subroom_id = xmltoa(xSubRoom->Attribute("id"), "-1");
            int subroom_id = xmltoi(xSubRoom->Attribute("id"), -1);

            std::string SubroomClosed = xmltoa(xSubRoom->Attribute("closed"), "0");
            std::string type          = xmltoa(xSubRoom->Attribute("class"), "subroom");

            //get the equation of the plane if any
            double A_x = xmltof(xSubRoom->Attribute("A_x"), 0.0);
            double B_y = xmltof(xSubRoom->Attribute("B_y"), 0.0);

            // assume either the old "C_z" or the new "C"
            double C_z = xmltof(xSubRoom->Attribute("C_z"), 0.0);
            C_z        = xmltof(xSubRoom->Attribute("C"), C_z);

            SubRoom * subroom = nullptr;

            if(type == "stair" || type == "escalator" || type == "idle_escalator") {
                if(xSubRoom->FirstChildElement("up") == nullptr) {
                    Logging::Error(fmt::format(
                        check_fmt("The attribute <up> and <down> are missing for the {:s}"), type));
                    Logging::Error("Check your geometry file");
                    return false;
                }
                double up_x   = xmltof(xSubRoom->FirstChildElement("up")->Attribute("px"), 0.0);
                double up_y   = xmltof(xSubRoom->FirstChildElement("up")->Attribute("py"), 0.0);
                double down_x = xmltof(xSubRoom->FirstChildElement("down")->Attribute("py"), 0.0);
                double down_y = xmltof(xSubRoom->FirstChildElement("down")->Attribute("py"), 0.0);
                subroom       = new Stair();
                ((Stair *) subroom)->SetUp(Point(up_x, up_y));
                ((Stair *) subroom)->SetDown(Point(down_x, down_y));
            } else if(type == "escalator_up") {
                if(xSubRoom->FirstChildElement("up") == nullptr) {
                    Logging::Error(fmt::format(
                        check_fmt("The attribute <up> and <down> are missing for the {:s}"), type));
                    Logging::Error("Check your geometry file");
                    return false;
                }
                double up_x   = xmltof(xSubRoom->FirstChildElement("up")->Attribute("px"), 0.0);
                double up_y   = xmltof(xSubRoom->FirstChildElement("up")->Attribute("py"), 0.0);
                double down_x = xmltof(xSubRoom->FirstChildElement("down")->Attribute("py"), 0.0);
                double down_y = xmltof(xSubRoom->FirstChildElement("down")->Attribute("py"), 0.0);
                subroom       = new Escalator();
                ((Escalator *) subroom)->SetUp(Point(up_x, up_y));
                ((Escalator *) subroom)->SetDown(Point(down_x, down_y));
                ((Escalator *) subroom)->SetEscalatorUp();
                _configuration->set_has_directional_escalators(true);
            } else if(type == "escalator_down") {
                if(xSubRoom->FirstChildElement("up") == nullptr) {
                    Logging::Error(fmt::format(
                        check_fmt("The attribute <up> and <down> are missing for the {:s}"), type));
                    Logging::Error("Check your geometry file");
                    return false;
                }
                double up_x   = xmltof(xSubRoom->FirstChildElement("up")->Attribute("px"), 0.0);
                double up_y   = xmltof(xSubRoom->FirstChildElement("up")->Attribute("py"), 0.0);
                double down_x = xmltof(xSubRoom->FirstChildElement("down")->Attribute("py"), 0.0);
                double down_y = xmltof(xSubRoom->FirstChildElement("down")->Attribute("py"), 0.0);
                subroom       = new Escalator();
                ((Escalator *) subroom)->SetUp(Point(up_x, up_y));
                ((Escalator *) subroom)->SetDown(Point(down_x, down_y));
                ((Escalator *) subroom)->SetEscalatorDown();
                _configuration->set_has_directional_escalators(true);
            } else {
                //normal subroom or corridor
                subroom = new NormalSubRoom();
            }

            subroom->SetType(type);
            subroom->SetPlanEquation(A_x, B_y, C_z);
            subroom->SetRoomID(room->GetID());
            subroom->SetSubRoomID(subroom_id);

            //static int p_id=1;
            //cout<<endl<<"wall polygon: "<< p_id++<<endl;
            //looking for polygons (walls)
            for(TiXmlElement * xPolyVertices = xSubRoom->FirstChildElement("polygon");
                xPolyVertices;
                xPolyVertices = xPolyVertices->NextSiblingElement("polygon")) {
                std::string wall_type = xmltoa(xPolyVertices->Attribute("type"), "wall");
                for(TiXmlElement * xVertex = xPolyVertices->FirstChildElement("vertex");
                    xVertex && xVertex != xPolyVertices->LastChild("vertex");
                    xVertex = xVertex->NextSiblingElement("vertex")) {
                    double x1 = xmltof(xVertex->Attribute("px"));
                    double y1 = xmltof(xVertex->Attribute("py"));
                    double x2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("px"));
                    double y2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("py"));
                    subroom->AddWall(Wall(Point(x1, y1), Point(x2, y2), wall_type));
                    //printf("%0.2f %0.2f %0.2f %0.2f\n",x1,y1,x2,y2);
                }
            }

            //looking for obstacles
            for(TiXmlElement * xObstacle = xSubRoom->FirstChildElement("obstacle"); xObstacle;
                xObstacle                = xObstacle->NextSiblingElement("obstacle")) {
                int id        = xmltoi(xObstacle->Attribute("id"), -1);
                double height = xmltof(xObstacle->Attribute("height"), 0);
                //double ObstClosed = xmltof(xObstacle->Attribute("closed"), 0);
                std::string ObstCaption = xmltoa(xObstacle->Attribute("caption"), "-1");

                Obstacle * obstacle = new Obstacle();
                obstacle->SetId(id);
                obstacle->SetCaption(ObstCaption);
                //obstacle->SetClosed(ObstClosed);
                obstacle->SetHeight(height);

                //looking for polygons (walls)
                for(TiXmlElement * xPolyVertices = xObstacle->FirstChildElement("polygon");
                    xPolyVertices;
                    xPolyVertices = xPolyVertices->NextSiblingElement("polygon")) {
                    for(TiXmlElement * xVertex = xPolyVertices->FirstChildElement("vertex");
                        xVertex && xVertex != xPolyVertices->LastChild("vertex");
                        xVertex = xVertex->NextSiblingElement("vertex")) {
                        double x1 = xmltof(xVertex->Attribute("px"));
                        double y1 = xmltof(xVertex->Attribute("py"));
                        double x2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("px"));
                        double y2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("py"));
                        obstacle->AddWall(Wall(Point(x1, y1), Point(x2, y2)));
                    }
                }
                subroom->AddObstacle(obstacle);
            }
            room->AddSubRoom(subroom);
        }
        //parsing the crossings
        TiXmlNode * xCrossingsNode = xRoom->FirstChild("crossings");
        if(xCrossingsNode)
            for(TiXmlElement * xCrossing = xCrossingsNode->FirstChildElement("crossing"); xCrossing;
                xCrossing                = xCrossing->NextSiblingElement("crossing")) {
                int id      = xmltoi(xCrossing->Attribute("id"), -1);
                int sub1_id = xmltoi(xCrossing->Attribute("subroom1_id"), -1);
                int sub2_id = xmltoi(xCrossing->Attribute("subroom2_id"), -1);

                double x1 = xmltof(xCrossing->FirstChildElement("vertex")->Attribute("px"));
                double y1 = xmltof(xCrossing->FirstChildElement("vertex")->Attribute("py"));
                double x2 = xmltof(xCrossing->LastChild("vertex")->ToElement()->Attribute("px"));
                double y2 = xmltof(xCrossing->LastChild("vertex")->ToElement()->Attribute("py"));

                Crossing * c = new Crossing();
                c->SetID(id);
                c->SetPoint1(Point(x1, y1));
                c->SetPoint2(Point(x2, y2));

                c->SetSubRoom1(room->GetSubRoom(sub1_id));
                c->SetSubRoom2(room->GetSubRoom(sub2_id));
                c->SetRoom1(room);
                building->AddCrossing(c);

                room->GetSubRoom(sub1_id)->AddCrossing(c);
                room->GetSubRoom(sub2_id)->AddCrossing(c);
            }

        building->AddRoom(room);
    }

    // all rooms are read, now proceed with transitions
    TiXmlNode * xTransNode = xRootNode->FirstChild("transitions");
    if(xTransNode) {
        for(TiXmlElement * xTrans = xTransNode->FirstChildElement("transition"); xTrans;
            xTrans                = xTrans->NextSiblingElement("transition")) {
            Transition * t = parseTransitionNode(xTrans, building);
            building->AddTransition(t);
        }
        TiXmlNode * xNodeFile = xTransNode->FirstChild("file");
        if(xNodeFile) {
            fs::path p(_configuration->GetProjectRootDir());
            std::string transFilename = xNodeFile->FirstChild()->ValueStr();
            p /= transFilename;
            transFilename = p.string();

            Logging::Info(
                fmt::format(check_fmt("Parsing transition from file <{:s}>"), transFilename));
            TiXmlDocument docTrans(transFilename);
            if(!docTrans.LoadFile()) {
                Logging::Error("Could not parse the transitions file.");
                Logging::Error(fmt::format(check_fmt("{:s}"), docTrans.ErrorDesc()));
                return false;
            }
            TiXmlElement * xRootNodeTrans = docTrans.RootElement();
            if(!xRootNodeTrans) {
                Logging::Error("Root element does not exist in transitions file.");
                return false;
            }
            if(xRootNodeTrans->ValueStr() != "JPScore") {
                Logging::Error("Parsing transitions file. Root element value is not 'JPScore'.");
                return false;
            }
            TiXmlNode * xTransNodeFile = xRootNodeTrans->FirstChild("transitions");
            if(!xTransNodeFile) {
                Logging::Error("No Transitions in file found.");
                return false;
            }
            for(TiXmlElement * xTrans = xTransNodeFile->FirstChildElement("transition"); xTrans;
                xTrans                = xTrans->NextSiblingElement("transition")) {
                Transition * t = parseTransitionNode(xTrans, building);
                building->AddTransition(t);
            }
        } else {
            Logging::Info("Not parsing transition from file");
        }
        Logging::Info(
            fmt::format(check_fmt("Got {} transitions"), building->GetAllTransitions().size()));

    } // xTransNode
    Logging::Info("Loading building file successful.");

    //everything went fine
    return true;
}

bool GeoFileParser::LoadRoutingInfo(Building * building)
{
    //TODO read schedule

    TiXmlDocument docRouting(_configuration->GetProjectFile().string());
    if(!docRouting.LoadFile()) {
        Logging::Error(fmt::format(check_fmt("{:s}"), docRouting.ErrorDesc()));
        Logging::Error("Could not parse the routing file");
        return false;
    }

    TiXmlElement * xRootNode = docRouting.RootElement();
    if(!xRootNode) {
        Logging::Error("Root element does not exist");
        return false;
    }

    if(!xRootNode->FirstChild("routing")) {
        return true; // no extra routing information
    }


    //load goals and routes
    TiXmlNode * xGoalsNode = xRootNode->FirstChild("routing")->FirstChild("goals");
    if(xGoalsNode) {
        // ---- parse goals from inifile
        for(TiXmlElement * e = xGoalsNode->FirstChildElement("goal"); e;
            e                = e->NextSiblingElement("goal")) {
            Goal * goal = parseGoalNode(e);
            if(goal) {
                building->AddGoal(goal);
                _configuration->GetRoutingEngine()->AddFinalDestinationID(goal->GetId());
            }
        }

        // ---- parse waiting areas from inifile
        for(TiXmlElement * e = xGoalsNode->FirstChildElement("waiting_area"); e;
            e                = e->NextSiblingElement("waiting_area")) {
            Goal * goal = parseWaitingAreaNode(e);
            if(goal) {
                building->AddGoal(goal);
                _configuration->GetRoutingEngine()->AddFinalDestinationID(goal->GetId());
            }
        }
        // ---- parse goals/waiting areas from external file
        TiXmlNode * xGoalsNodeFile = xGoalsNode->FirstChild("file");
        if(xGoalsNodeFile) {
            fs::path p(_configuration->GetProjectRootDir());
            std::string goalFilename = xGoalsNodeFile->FirstChild()->ValueStr();
            p /= goalFilename;
            goalFilename = p.string();
            Logging::Info(fmt::format(check_fmt("Goal file <{:s}> will be parsed."), goalFilename));
            TiXmlDocument docGoal(goalFilename);
            if(!docGoal.LoadFile()) {
                Logging::Error(fmt::format(check_fmt("{}"), docGoal.ErrorDesc()));
                Logging::Error("Could not parse the goal file.");
                return false;
            }
            TiXmlElement * xRootNodeGoal = docGoal.RootElement();
            if(!xRootNodeGoal) {
                Logging::Error("Root element does not exist in goal file.");
                return false;
            }
            if(xRootNodeGoal->ValueStr() != "JPScore") {
                Logging::Error("Parsing goal file. Root element value is not 'JPScore'.");
                return false;
            }
            TiXmlNode * xGoal = xRootNodeGoal->FirstChild("goals");
            if(!xGoal) {
                Logging::Error("No Goals in file found.");
                return false;
            }
            for(TiXmlElement * e = xGoal->FirstChildElement("goal"); e;
                e                = e->NextSiblingElement("goal")) {
                Goal * goal = parseGoalNode(e);
                if(goal) {
                    building->AddGoal(goal);
                    _configuration->GetRoutingEngine()->AddFinalDestinationID(goal->GetId());
                }
            }

            for(TiXmlElement * e = xGoal->FirstChildElement("waiting_area"); e;
                e                = e->NextSiblingElement("waiting_area")) {
                Goal * goal = parseWaitingAreaNode(e);
                if(goal) {
                    building->AddGoal(goal);
                    _configuration->GetRoutingEngine()->AddFinalDestinationID(goal->GetId());
                }
            }


        } else
            Logging::Info("Goal file not parsed");
    } //xgoalsNode

    Logging::Info("Done with loading extra routing information");
    return true;
}

bool GeoFileParser::parseDoorNode(TiXmlElement * xDoor, int id, Building * building)
{
    bool result = false; // this return value is not needed.
                         // maybe in the future it might be...
    Logging::Info("Parsed Door:");
    Logging::Info(fmt::format(check_fmt(">> ID: {}"), id));
    //------------------ state
    std::string stateStr = xmltoa(xDoor->Attribute("state"), "open");
    DoorState state      = StringToDoorState(stateStr);
    //store transition in a map and call getTransition/getCrossing
    switch(state) {
        case DoorState::OPEN:
            building->GetTransition(id)->Open();
            break;
        case DoorState::CLOSE:
            building->GetTransition(id)->Close();
            break;
        case DoorState::TEMP_CLOSE:
            building->GetTransition(id)->TempClose();
            break;
        case DoorState::Error:
            Logging::Warning(fmt::format(
                check_fmt("Unknown door state: <{}>. open, close or temp_close. Default: open"),
                stateStr));
            building->GetTransition(id)->Open();
            break;
    }

    Logging::Info(fmt::format(check_fmt(">> state: {}"), stateStr));

    //------------------ outflow
    double outflow = xmltof(xDoor->Attribute("outflow"), -1.0);
    if(outflow >= 0) {
        building->GetTransition(id)->SetOutflowRate(outflow);
        Logging::Info(fmt::format(check_fmt(">> ouflow: {:.2f}"), outflow));
    }
    //----------------- dt
    double DT = xmltof(xDoor->Attribute("dt"), -1.0);
    if(DT >= 0) {
        building->GetTransition(id)->SetDT(DT);
    }
    //----------------- dn
    int DN = xmltof(xDoor->Attribute("dn"), -1.0);
    if(DN >= 0) {
        building->GetTransition(id)->SetDN(DN);
        Logging::Info(fmt::format(check_fmt(">> dn: {}"), DN));
    }

    //------------------ max door usage
    int mdu = xmltof(xDoor->Attribute("max_agents"), -1);
    if(mdu >= 0) {
        building->GetTransition(id)->SetMaxDoorUsage(mdu);
        Logging::Info(fmt::format(check_fmt(">> max_agents: {}"), mdu));
    }
    //-----------------
    result = true;
    return result;
}

bool GeoFileParser::LoadTrafficInfo(Building * building)
{
    Logging::Info("Loading the traffic info");

    TiXmlDocument doc(_configuration->GetProjectFile().string());
    if(!doc.LoadFile()) {
        Logging::Error(fmt::format(check_fmt("{}"), doc.ErrorDesc()));
        Logging::Error("Could not parse the project file");
        return false;
    }

    TiXmlNode * xRootNode = doc.RootElement()->FirstChild("traffic_constraints");
    if(!xRootNode) {
        Logging::Warning("Could not find any traffic information");
        return true;
    }

    //processing the rooms node
    TiXmlNode * xRoomsNode = xRootNode->FirstChild("rooms");
    if(xRoomsNode)
        for(TiXmlElement * xRoom = xRoomsNode->FirstChildElement("room"); xRoom;
            xRoom                = xRoom->NextSiblingElement("room")) {
            double id         = xmltof(xRoom->Attribute("room_id"), -1);
            std::string state = xmltoa(xRoom->Attribute("state"), "good");
            RoomState status  = (state == "good") ? ROOM_CLEAN : ROOM_SMOKED;
            building->GetRoom(id)->SetState(status);
        }
    else
        Logging::Info("No room info found in inifile");

    //processing the doors node
    TiXmlNode * xDoorsNode = xRootNode->FirstChild("doors");
    if(xDoorsNode) {
        bool res_parseDoor;
        for(TiXmlElement * xDoor = xDoorsNode->FirstChildElement("door"); xDoor;
            xDoor                = xDoor->NextSiblingElement("door")) {
            int id = xmltoi(xDoor->Attribute("trans_id"), -1);
            if(id != -1 && building->GetTransition(id)) {
                res_parseDoor = parseDoorNode(xDoor, id, building);
                if(!res_parseDoor)
                    return false;
            }
        } //for xDoor
    } else
        Logging::Info("No door info found in inifile.");
    // processing file node
    TiXmlNode * xFileNode = xRootNode->FirstChild("file");
    if(xFileNode) {
        fs::path p(_configuration->GetProjectRootDir());
        std::string trafficFilename = xFileNode->FirstChild()->ValueStr();
        p /= trafficFilename;
        trafficFilename = p.string();
        Logging::Info(fmt::format(check_fmt("Traffic file found <{}>"), trafficFilename));
        TiXmlDocument docTraffic(trafficFilename);
        if(!docTraffic.LoadFile()) {
            Logging::Error(fmt::format(check_fmt("{}"), docTraffic.ErrorDesc()));
            Logging::Error("Could not parse the traffic file.");
            return false;
        }
        TiXmlElement * xRootNodeTraffic = docTraffic.RootElement();
        if(!xRootNodeTraffic) {
            Logging::Error("Root element does not exist.");
            return false;
        }

        if(xRootNodeTraffic->ValueStr() != "JPScore") {
            Logging::Error("Root element value is not 'JPScore'.");
            return false;
        }
        TiXmlNode * xTraffic = xRootNodeTraffic->FirstChild("traffic_constraints");
        if(!xTraffic) {
            Logging::Error("No traffic constraints in file found.");
            return false;
        }
        TiXmlNode * xDoorsNodeF = xTraffic->FirstChild("doors");
        if(xDoorsNodeF) {
            bool res_parseDoor;
            for(TiXmlElement * xDoor = xDoorsNodeF->FirstChildElement("door"); xDoor;
                xDoor                = xDoor->NextSiblingElement("door")) {
                int id = xmltoi(xDoor->Attribute("trans_id"), -1);
                if(id != -1 && building->GetTransition(id)) {
                    res_parseDoor = parseDoorNode(xDoor, id, building);
                    if(!res_parseDoor)
                        return false;
                }
            } //for xDoor
        } else
            Logging::Info("No door info found in traffic file");
    } else
        Logging::Info("No traffic file found.");

    Logging::Info("Done with loading traffic info file");
    return true;
}

Transition * GeoFileParser::parseTransitionNode(TiXmlElement * xTrans, Building * building)
{
    int id = xmltoi(xTrans->Attribute("id"), -1);
    // string caption = "door " + id;
    std::string caption = "door ";
    caption += std::to_string(id);
    caption          = xmltoa(xTrans->Attribute("caption"), caption.c_str());
    int room1_id     = xmltoi(xTrans->Attribute("room1_id"), -1);
    int room2_id     = xmltoi(xTrans->Attribute("room2_id"), -1);
    int subroom1_id  = xmltoi(xTrans->Attribute("subroom1_id"), -1);
    int subroom2_id  = xmltoi(xTrans->Attribute("subroom2_id"), -1);
    std::string type = xmltoa(xTrans->Attribute("type"), "normal");

    double x1 = xmltof(xTrans->FirstChildElement("vertex")->Attribute("px"));
    double y1 = xmltof(xTrans->FirstChildElement("vertex")->Attribute("py"));

    double x2 = xmltof(xTrans->LastChild("vertex")->ToElement()->Attribute("px"));
    double y2 = xmltof(xTrans->LastChild("vertex")->ToElement()->Attribute("py"));

    Transition * t = new Transition();
    t->SetID(id);
    t->SetCaption(caption);
    t->SetPoint1(Point(x1, y1));
    t->SetPoint2(Point(x2, y2));
    t->SetType(type);
    //--- danger area
    if(room1_id != -1 && subroom1_id != -1) {
        //Room* room = _rooms[room1_id];
        Room * room       = building->GetRoom(room1_id);
        SubRoom * subroom = room->GetSubRoom(subroom1_id);

        //subroom->AddGoalID(t->GetUniqueID());
        //MPI
        room->AddTransitionID(t->GetUniqueID());
        t->SetRoom1(room);
        t->SetSubRoom1(subroom);

        //new implementation
        subroom->AddTransition(t);
    }
    if(room2_id != -1 && subroom2_id != -1) {
        Room * room       = building->GetRoom(room2_id);
        SubRoom * subroom = room->GetSubRoom(subroom2_id);
        //subroom->AddGoalID(t->GetUniqueID());
        //MPI
        room->AddTransitionID(t->GetUniqueID());
        t->SetRoom2(room);
        t->SetSubRoom2(subroom);

        //new implementation
        subroom->AddTransition(t);
    }
    return t;
}

Goal * GeoFileParser::parseGoalNode(TiXmlElement * e)
{
    Logging::Info("Loading goal");
    int id              = xmltoi(e->Attribute("id"), -1);
    int isFinal         = std::string(e->Attribute("final")) == "true" ? true : false;
    std::string caption = xmltoa(e->Attribute("caption"), "-1");
    int room_id         = xmltoi(e->Attribute("room_id"), -1);
    int subroom_id      = xmltoi(e->Attribute("subroom_id"), -1);
    Logging::Info(fmt::format(check_fmt("Goal id: {}"), id));
    Logging::Info(fmt::format(check_fmt("Goal caption: {}"), caption));
    Logging::Info(fmt::format(check_fmt("Goal room_id: {}"), room_id));
    Logging::Info(fmt::format(check_fmt("Goal subroom_id: {}"), subroom_id));
    Goal * goal = new Goal();
    goal->SetId(id);
    goal->SetCaption(caption);
    goal->SetIsFinalGoal(isFinal);
    goal->SetRoomID(room_id);
    goal->SetSubRoomID(subroom_id);

    //looking for polygons (walls)
    for(TiXmlElement * xPolyVertices = e->FirstChildElement("polygon"); xPolyVertices;
        xPolyVertices                = xPolyVertices->NextSiblingElement("polygon")) {
        for(TiXmlElement * xVertex = xPolyVertices->FirstChildElement("vertex");
            xVertex && xVertex != xPolyVertices->LastChild("vertex");
            xVertex = xVertex->NextSiblingElement("vertex")) {
            double x1 = xmltof(xVertex->Attribute("px"));
            double y1 = xmltof(xVertex->Attribute("py"));
            double x2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("px"));
            double y2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("py"));
            goal->AddWall(Wall(Point(x1, y1), Point(x2, y2)));
        }
    }

    if(!goal->ConvertLineToPoly()) {
        Logging::Error(fmt::format(check_fmt("Parsing polygon of goal {}"), id));
        return nullptr;
    }
    Logging::Info(fmt::format(check_fmt("Finished parsing goal {}"), id));
    return goal;
}

Goal * GeoFileParser::parseWaitingAreaNode(TiXmlElement * e)
{
    Logging::Info("Loading Waiting Area");

    WaitingArea * wa = new WaitingArea();

    // Read mandatory values and check for valid values, on fail write error
    // Read id and check for correct value
    if(const char * attribute = e->Attribute("id"); attribute) {
        if(int value = xmltoi(attribute, -1); value > -1 && attribute == std::to_string(value)) {
            wa->SetId(value);
        } else {
            Logging::Error(" waiting area id set but not an integer");
            delete wa;
            return nullptr;
        }
    } else {
        Logging::Error(" waiting area id required");
        delete wa;
        return nullptr;
    }

    // Read room_id and check for correct value
    if(const char * attribute = e->Attribute("room_id"); attribute) {
        if(int value = xmltoi(attribute, -1); value > -1 && attribute == std::to_string(value)) {
            wa->SetRoomID(value);
        } else {
            Logging::Error(fmt::format(
                check_fmt("waiting area {:d}: room_id set but not an integer"), wa->GetId()));
            delete wa;
            return nullptr;
        }
    } else {
        Logging::Error(fmt::format(check_fmt("waiting area {:d}: room_id required"), wa->GetId()));
        delete wa;
        return nullptr;
    }

    // Read subroom_id and check for correct value
    if(const char * attribute = e->Attribute("subroom_id"); attribute) {
        if(int value = xmltoi(attribute, -1); value > -1 && attribute == std::to_string(value)) {
            wa->SetSubRoomID(value);
        } else {
            Logging::Error(fmt::format(
                check_fmt("waiting area {:d}: subroom_id set but not an integer"), wa->GetId()));
            delete wa;
            return nullptr;
        }
    } else {
        Logging::Error(
            fmt::format(check_fmt("waiting area {:d}: subroom_id required"), wa->GetId()));
        delete wa;
        return nullptr;
    }

    // Read caption and check if correct value
    if(const char * attribute = e->Attribute("caption"); attribute) {
        if(std::string value = xmltoa(attribute, ""); !value.empty()) {
            wa->SetCaption(value);
        } else {
            wa->SetCaption("WA " + std::to_string(wa->GetId()));
        }
    }

    Logging::Info(fmt::format(check_fmt("Goal id: {:d}"), wa->GetId()));
    Logging::Info(fmt::format(check_fmt("Goal caption: {:s}"), wa->GetCaption().c_str()));
    Logging::Info(fmt::format(check_fmt("Goal room_id: {:d}"), wa->GetRoomID()));
    Logging::Info(fmt::format(check_fmt("Goal subroom_id: {:d}"), wa->GetSubRoomID()));

    // Read optional values and check for valid values, on fail write error
    // Read min_peds and check if correct value
    if(const char * attribute = e->Attribute("min_peds"); attribute) {
        if(int value = xmltoi(attribute, -1); value > 0 && attribute == std::to_string(value)) {
            wa->SetMinNumPed(value);
        } else {
            Logging::Warning(fmt::format(
                check_fmt("waiting area {:d}: input for min_peds should be positive integer."),
                wa->GetId()));
        }
    }

    // Read max_peds and check if correct value
    if(const char * attribute = e->Attribute("max_peds"); attribute) {
        if(int value = xmltoi(attribute, -1); value > 0 && attribute == std::to_string(value)) {
            wa->SetMaxNumPed(value);
        } else {
            Logging::Warning(fmt::format(
                check_fmt("waiting area {:d}: input for max_peds should be positive integer."),
                wa->GetId()));
        }
    }

    // Read waiting_time and check if correct value
    if(const char * attribute = e->Attribute("waiting_time"); attribute) {
        if(int value = xmltoi(attribute, -1); value >= 0 && attribute == std::to_string(value)) {
            wa->SetWaitingTime(value);
        } else {
            Logging::Warning(fmt::format(
                check_fmt("waiting area {:d}: input for waiting_time should be positive integer."),
                wa->GetId()));
        }
    }

    // Read transition_id and check if correct value
    if(const char * attribute = e->Attribute("transition_id"); attribute) {
        if(int value = xmltoi(attribute, -1); value > -1 && attribute == std::to_string(value)) {
            wa->SetTransitionID(value);
        } else {
            Logging::Warning(fmt::format(
                check_fmt("waiting area {:d}: input for transition_id should be positive integer."),
                wa->GetId()));
        }
    }

    // Read is_open and check if correct value
    if(const char * attribute = e->Attribute("is_open"); attribute) {
        std::string in = xmltoa(attribute, "false");
        std::transform(in.begin(), in.end(), in.begin(), ::tolower);

        if(in == "false") {
            wa->SetOpen(false);
        } else if(in == "true") {
            wa->SetOpen(true);
        } else {
            wa->SetOpen(true);
            Logging::Warning(fmt::format(
                check_fmt("waiting area {:d}: input for is_open neither <true> nor <false>. "
                          "Default <true> is used."),
                wa->GetId()));
        }
    }

    // Read global_timer and check if correct value
    if(const char * attribute = e->Attribute("global_timer"); attribute) {
        std::string in = xmltoa(attribute, "false");
        std::transform(in.begin(), in.end(), in.begin(), ::tolower);

        if(in == "false") {
            wa->SetGlobalTimer(false);
        } else if(in == "true") {
            wa->SetGlobalTimer(true);
        } else {
            wa->SetGlobalTimer(false);
            Logging::Warning(fmt::format(
                check_fmt("waiting area {:d}: input for global_timer neither <true> nor <false>. "
                          "Default <true> is used."),
                wa->GetId()));
        }
    }

    // Additional checks:
    const bool waitPed =
        (wa->GetMinNumPed() > 0 && wa->GetMaxNumPed() > 0 && wa->GetWaitingTime() >= 0.);
    const bool waitDoor = (wa->GetTransitionID() > 0);

    // Either (minPed, maxPed, waitingTime) OR transitionID are set
    if(!waitPed && !waitDoor) {
        Logging::Error(fmt::format(
            check_fmt("waiting area {:d}: min_peds, max_peds, waiting_time, transition_id not set "
                      "properly. Set either (min_peds, max_peds, waiting_time) or transition_id."),
            wa->GetId()));
        delete wa;
        return nullptr;
    }

    // If (minPed, maxPed, waitingTime) AND transitionID are set only
    // transitionID is considered
    if(waitPed && waitDoor) {
        Logging::Warning(fmt::format(
            check_fmt("waiting area {:d}: min_peds, max_peds and waiting_time not considered since "
                      "transition_id set."),
            wa->GetId()));
    }

    // Read the succeeding goals of waiting area
    std::map<int, double> nextGoals;

    //looking for next_wa
    for(TiXmlElement * nextWa = e->FirstChildElement("next_wa"); nextWa;
        nextWa                = nextWa->NextSiblingElement("next_wa")) {
        int nextWaId   = xmltoi(nextWa->Attribute("id"), std::numeric_limits<int>::min());
        double nextWaP = xmltof(nextWa->Attribute("p"), std::numeric_limits<double>::min());

        if(nextWaId == std::numeric_limits<int>::min() ||
           nextWaP == std::numeric_limits<double>::min()) {
            Logging::Error(fmt::format(
                check_fmt("waiting area {:d}: check next_wa id or p not set properly."),
                wa->GetId()));
            delete wa;
            return nullptr;
        }

        if(nextWaId < -2) {
            Logging::Error(fmt::format(
                check_fmt("waiting area {:d}: check next_wa id should be positive integer."),
                wa->GetId()));
            delete wa;
            return nullptr;
        }

        if(nextWaP < 0. || nextWaP > 1. + 1e-5) {
            Logging::Error(fmt::format(
                check_fmt("waiting area {:d}: check next_wa p should be in [0, 1]."), wa->GetId()));
            delete wa;
            return nullptr;
        }
        nextGoals.insert(std::pair<int, double>(nextWaId, nextWaP));
    }

    if(!wa->SetNextGoals(nextGoals)) {
        Logging::Error(fmt::format(
            check_fmt("waiting area {:d}: check probabilities sum of p over all next_wa ids != 1."),
            wa->GetId()));
        delete wa;
        return nullptr;
    }

    //Read boundaries of waiting area
    for(TiXmlElement * xPolyVertices = e->FirstChildElement("polygon"); xPolyVertices;
        xPolyVertices                = xPolyVertices->NextSiblingElement("polygon")) {
        for(TiXmlElement * xVertex = xPolyVertices->FirstChildElement("vertex");
            xVertex && xVertex != xPolyVertices->LastChild("vertex");
            xVertex = xVertex->NextSiblingElement("vertex")) {
            double x1 = xmltof(xVertex->Attribute("px"));
            double y1 = xmltof(xVertex->Attribute("py"));
            double x2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("px"));
            double y2 = xmltof(xVertex->NextSiblingElement("vertex")->Attribute("py"));
            wa->AddWall(Wall(Point(x1, y1), Point(x2, y2)));
        }
    }

    // Check if boundaries area a valid polygon
    if(!wa->ConvertLineToPoly()) {
        Logging::Error(fmt::format(
            check_fmt("waiting area {:d}: parsing polygon of waiting area."), wa->GetId()));
        delete wa;
        return nullptr;
    }

    Logging::Info(
        fmt::format(check_fmt("waiting area {:d}: finished parsing waiting area."), wa->GetId()));
    return wa;
}

bool GeoFileParser::LoadTrainInfo(Building * building)
{
    Logging::Info("Loading the train info");

    TiXmlDocument doc(_configuration->GetProjectFile().string());
    if(!doc.LoadFile()) {
        Logging::Error(fmt::format(check_fmt("{}"), doc.ErrorDesc()));
        Logging::Error("Could not parse the project file");
        return false;
    }
    TiXmlElement * xRootNode = doc.RootElement();
    if(!xRootNode) {
        Logging::Error("Root element does not exist");
        return false;
    }
    if(!xRootNode->FirstChild("train_constraints")) {
        Logging::Warning("No train constraints were found. Continue.");
    }
    bool resTTT  = true;
    bool resType = true;
    if(xRootNode->FirstChild("train_constraints")) {
        resTTT  = LoadTrainTimetable(building, xRootNode);
        resType = LoadTrainType(building, xRootNode);
    }

    return (resTTT && resType);
}
bool GeoFileParser::LoadTrainTimetable(Building * building, TiXmlElement * xRootNode)
{
    TiXmlNode * xTTTNode =
        xRootNode->FirstChild("train_constraints")->FirstChild("train_time_table");
    std::string TTTFilename;
    if(xTTTNode) {
        fs::path p(_configuration->GetProjectRootDir());
        TTTFilename = xTTTNode->FirstChild()->ValueStr();
        p /= TTTFilename;
        TTTFilename = p.string();
        Logging::Info(
            fmt::format(check_fmt("Train Timetable file <{}> will be parsed"), TTTFilename));
    } else
        return true;


    TiXmlDocument docTTT(TTTFilename);
    if(!docTTT.LoadFile()) {
        Logging::Error(fmt::format(check_fmt("{}"), docTTT.ErrorDesc()));
        Logging::Error("Could not parse the train timetable file.");
        return false;
    }
    TiXmlElement * xTTT = docTTT.RootElement();
    if(!xTTT) {
        Logging::Error("Root element does not exist in TTT file.");
        return false;
    }
    if(xTTT->ValueStr() != "train_time_table") {
        Logging::Error(
            "Parsing train timetable file. Root element value is not 'train_time_table'.");
        return false;
    }
    for(TiXmlElement * e = xTTT->FirstChildElement("train"); e;
        e                = e->NextSiblingElement("train")) {
        std::shared_ptr<TrainTimeTable> TTT = parseTrainTimeTableNode(e);

        if(TTT) {
            building->AddTrainTimeTable(TTT);
        }
    }
    return true;
}
bool GeoFileParser::LoadTrainType(Building * building, TiXmlElement * xRootNode)
{
    TiXmlNode * xTTNode = xRootNode->FirstChild("train_constraints")->FirstChild("train_types");
    std::string TTFilename;
    if(xTTNode) {
        fs::path p(_configuration->GetProjectRootDir());
        TTFilename = xTTNode->FirstChild()->ValueStr();
        p /= TTFilename;
        TTFilename = p.string();
        Logging::Info(fmt::format(check_fmt("Train Type file <{}> will be parsed"), TTFilename));
    } else
        return true;


    TiXmlDocument docTT(TTFilename);
    if(!docTT.LoadFile()) {
        Logging::Error(fmt::format(check_fmt("{}"), docTT.ErrorDesc()));
        Logging::Error("Could not parse the train type file.");
        return false;
    }
    TiXmlElement * xTT = docTT.RootElement();
    if(!xTT) {
        Logging::Error("Root element does not exist in TT file.");
        return false;
    }
    if(xTT->ValueStr() != "train_type") {
        Logging::Error("Parsing train type file. Root element value is not 'train_type'.");
        return false;
    }
    for(TiXmlElement * e = xTT->FirstChildElement("train"); e; e = e->NextSiblingElement("train")) {
        std::shared_ptr<TrainType> TT = parseTrainTypeNode(e);
        if(TT) {
            building->AddTrainType(TT);
        }
    }
    return true;
}

std::shared_ptr<TrainTimeTable> GeoFileParser::parseTrainTimeTableNode(TiXmlElement * e)
{
    Logging::Info("Loading train time table NODE");
    std::string caption = xmltoa(e->Attribute("caption"), "-1");
    int id              = xmltoi(e->Attribute("id"), -1);
    std::string type    = xmltoa(e->Attribute("type"), "-1");
    int room_id         = xmltoi(e->Attribute("room_id"), -1);
    int subroom_id      = xmltoi(e->Attribute("subroom_id"), -1);
    int platform_id     = xmltoi(e->Attribute("platform_id"), -1);
    float track_start_x = xmltof(e->Attribute("track_start_x"), -1);
    float track_start_y = xmltof(e->Attribute("track_start_y"), -1);
    float track_end_x   = xmltof(e->Attribute("track_end_x"), -1);
    float track_end_y   = xmltof(e->Attribute("track_end_y"), -1);

    float train_start_x = xmltof(e->Attribute("train_start_x"), -1);
    float train_start_y = xmltof(e->Attribute("train_start_y"), -1);
    float train_end_x   = xmltof(e->Attribute("train_end_x"), -1);
    float train_end_y   = xmltof(e->Attribute("train_end_y"), -1);

    float arrival_time   = xmltof(e->Attribute("arrival_time"), -1);
    float departure_time = xmltof(e->Attribute("departure_time"), -1);
    // @todo: check these values for correctness e.g. arrival < departure
    Logging::Info("Train time table:");
    Logging::Info(fmt::format(check_fmt("ID: {}"), id));
    Logging::Info(fmt::format(check_fmt("Type: {}"), type));
    Logging::Info(fmt::format(check_fmt("room_id: {}"), room_id));
    Logging::Info(fmt::format(check_fmt("subroom_id: {}"), subroom_id));
    Logging::Info(fmt::format(check_fmt("platform_id: {}"), platform_id));
    Logging::Info(
        fmt::format(check_fmt("track_start: [{:.2f}, {:.2f}]"), track_start_x, track_start_y));
    Logging::Info(fmt::format(check_fmt("track_end: [{:.2f}, {:.2f}]"), track_end_x, track_end_y));
    Logging::Info(fmt::format(check_fmt("arrival_time: {:.2f}"), arrival_time));
    Logging::Info(fmt::format(check_fmt("departure_time: {:.2f}"), departure_time));
    Point track_start(track_start_x, track_start_y);
    Point track_end(track_end_x, track_end_y);
    Point train_start(train_start_x, train_start_y);
    Point train_end(train_end_x, train_end_y);
    std::shared_ptr<TrainTimeTable> trainTimeTab = std::make_shared<TrainTimeTable>(TrainTimeTable{
        id,
        type,
        room_id,
        subroom_id,
        arrival_time,
        departure_time,
        track_start,
        track_end,
        train_start,
        train_end,
        platform_id,
        false,
        false,
    });

    return trainTimeTab;
}

std::shared_ptr<TrainType> GeoFileParser::parseTrainTypeNode(TiXmlElement * e)
{
    Logging::Info("Loading train type");
    // int T_id = xmltoi(e->Attribute("id"), -1);
    std::string type = xmltoa(e->Attribute("type"), "-1");
    int agents_max   = xmltoi(e->Attribute("agents_max"), -1);
    float length     = xmltof(e->Attribute("length"), -1);
    // std::shared_ptr<Transition> t = new Transition();
    // std::shared_ptr<Transition> doors;
    Transition t;
    std::vector<Transition> doors;

    for(TiXmlElement * xDoor = e->FirstChildElement("door"); xDoor;
        xDoor                = xDoor->NextSiblingElement("door")) {
        int D_id = xmltoi(xDoor->Attribute("id"), -1);
        float x1 = xmltof(xDoor->FirstChildElement("vertex")->Attribute("px"), -1);
        float y1 = xmltof(xDoor->FirstChildElement("vertex")->Attribute("py"), -1);
        float x2 = xmltof(xDoor->LastChild("vertex")->ToElement()->Attribute("px"), -1);
        float y2 = xmltof(xDoor->LastChild("vertex")->ToElement()->Attribute("py"), -1);
        Point start(x1, y1);
        Point end(x2, y2);
        float outflow = xmltof(xDoor->Attribute("outflow"), -1);
        float dn      = xmltoi(xDoor->Attribute("dn"), -1);
        t.SetID(D_id);
        t.SetCaption(type + std::to_string(D_id));
        t.SetPoint1(start);
        t.SetPoint2(end);
        t.SetOutflowRate(outflow);
        t.SetDN(dn);
        doors.push_back(t);
    }
    Logging::Info(fmt::format(check_fmt("type: {}"), type));
    Logging::Info(fmt::format(check_fmt("capacity: {}"), agents_max));
    Logging::Info(fmt::format(check_fmt("number of doors: {}"), doors.size()));
    for(auto d : doors) {
        Logging::Info(fmt::format(
            check_fmt("Door ({}): {} | {}"),
            d.GetID(),
            d.GetPoint1().toString(),
            d.GetPoint2().toString()));
    }

    std::shared_ptr<TrainType> Type = std::make_shared<TrainType>(TrainType{
        type,
        agents_max,
        length,
        doors,
    });
    return Type;
}

GeoFileParser::~GeoFileParser() {}
