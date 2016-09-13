//
// Created by ar.graf on 1/28/16.
//

#include "LocalFloorfieldViaFM.h"

LocalFloorfieldViaFM::LocalFloorfieldViaFM(){};
LocalFloorfieldViaFM::LocalFloorfieldViaFM(const Room* const roomArg,
               const Building* buildingArg,
               const double hxArg, const double hyArg,
               const double wallAvoidDistance, const bool useDistancefield) {
     //ctor
     _threshold = -1; //negative value means: ignore threshold
     _threshold = wallAvoidDistance;
     _building = buildingArg;
     _room = roomArg;

     if (hxArg != hyArg) std::cerr << "ERROR: hx != hy <=========";
     //parse building and create list of walls/obstacles (find xmin xmax, ymin, ymax, and add border?)
     Log->Write("INFO: \tStart Parsing: Room %d", roomArg->GetID());
     parseRoom(roomArg, hxArg, hyArg);
     Log->Write("INFO: \tFinished Parsing: Room %d", roomArg->GetID());
     //testoutput("AALineScan.vtk", "AALineScan.txt", dist2Wall);

     prepareForDistanceFieldCalculation(false);
     //here we need to draw blocker lines @todo: ar.graf
     //drawBlockerLines();
     Log->Write("INFO: \tGrid initialized: Walls in room %d", roomArg->GetID());

     calculateDistanceField(-1.); //negative threshold is ignored, so all distances get calculated. this is important since distances is used for slowdown/redirect
     Log->Write("INFO: \tGrid initialized: Walldistances in room %d", roomArg->GetID());

     setSpeed(useDistancefield); //use distance2Wall
     Log->Write("INFO: \tGrid initialized: Speed in room %d", roomArg->GetID());
     calculateFloorfield(_exitsFromScope, _cost, _neggrad);
     Log->Write("INFO: \tFloor field for \"goal -1\" done in room %d", roomArg->GetID());
};

//void LocalFloorfieldViaFM::getDirectionToGoalID(const int goalID){
//     std::cerr << "invalid call to LocalFloorfieldViaFM::getDirectionToGoalID!" << std::endl;
//};

void LocalFloorfieldViaFM::parseRoom(const Room* const roomArg,
      const double hxArg, const double hyArg)
{
     _room = roomArg;
     //init min/max before parsing
     double xMin = DBL_MAX;
     double xMax = -DBL_MAX;
     double yMin = xMin;
     double yMax = xMax;
     _costmap.clear();
     _neggradmap.clear();

     //create a list of walls
     //add all transition and put open doors at the beginning of "wall"
     std::map<int, Transition*> allTransitions;
     for (auto& itSubroom : _room->GetAllSubRooms()) {
          for (auto itTrans : itSubroom.second->GetAllTransitions()) {
               if (!allTransitions.count(itTrans->GetUniqueID())) {
                    allTransitions[itTrans->GetUniqueID()] = &(*itTrans);
                    if (itTrans->IsOpen()) {
                         _exitsFromScope.emplace_back(Line((Line) *itTrans));
                         _costmap.emplace(itTrans->GetUniqueID(), nullptr);
                         _neggradmap.emplace(itTrans->GetUniqueID(), nullptr);
                    }
               }
          }
     }
     _numOfExits = (unsigned int) _exitsFromScope.size();
     //put closed doors next, they are considered as walls later (index >= numOfExits)
     for (auto& trans : allTransitions) {
          if (!trans.second->IsOpen()) {
               _wall.emplace_back(Line ( (Line) *(trans.second)));
          }
     }

     for (const auto& itSubroom : _room->GetAllSubRooms()) {
          std::vector<Obstacle*> allObstacles = itSubroom.second->GetAllObstacles();
          for (std::vector<Obstacle*>::iterator itObstacles = allObstacles.begin(); itObstacles != allObstacles.end(); ++itObstacles) {

               std::vector<Wall> allObsWalls = (*itObstacles)->GetAllWalls();
               for (std::vector<Wall>::iterator itObsWall = allObsWalls.begin(); itObsWall != allObsWalls.end(); ++itObsWall) {
                    _wall.emplace_back(Line( (Line) *itObsWall));
                    // xMin xMax
                    if ((*itObsWall).GetPoint1()._x < xMin) xMin = (*itObsWall).GetPoint1()._x;
                    if ((*itObsWall).GetPoint2()._x < xMin) xMin = (*itObsWall).GetPoint2()._x;
                    if ((*itObsWall).GetPoint1()._x > xMax) xMax = (*itObsWall).GetPoint1()._x;
                    if ((*itObsWall).GetPoint2()._x > xMax) xMax = (*itObsWall).GetPoint2()._x;
                    // yMin yMax
                    if ((*itObsWall).GetPoint1()._y < yMin) yMin = (*itObsWall).GetPoint1()._y;
                    if ((*itObsWall).GetPoint2()._y < yMin) yMin = (*itObsWall).GetPoint2()._y;
                    if ((*itObsWall).GetPoint1()._y > yMax) yMax = (*itObsWall).GetPoint1()._y;
                    if ((*itObsWall).GetPoint2()._y > yMax) yMax = (*itObsWall).GetPoint2()._y;
               }
          }

          std::vector<Wall> allWalls = itSubroom.second->GetAllWalls();
          for (std::vector<Wall>::iterator itWall = allWalls.begin(); itWall != allWalls.end(); ++itWall) {
               _wall.emplace_back( Line( (Line) *itWall));
               // xMin xMax
               if ((*itWall).GetPoint1()._x < xMin) xMin = (*itWall).GetPoint1()._x;
               if ((*itWall).GetPoint2()._x < xMin) xMin = (*itWall).GetPoint2()._x;
               if ((*itWall).GetPoint1()._x > xMax) xMax = (*itWall).GetPoint1()._x;
               if ((*itWall).GetPoint2()._x > xMax) xMax = (*itWall).GetPoint2()._x;
               // yMin yMax
               if ((*itWall).GetPoint1()._y < yMin) yMin = (*itWall).GetPoint1()._y;
               if ((*itWall).GetPoint2()._y < yMin) yMin = (*itWall).GetPoint2()._y;
               if ((*itWall).GetPoint1()._y > yMax) yMax = (*itWall).GetPoint1()._y;
               if ((*itWall).GetPoint2()._y > yMax) yMax = (*itWall).GetPoint2()._y;
          }
          const vector<Crossing*>& allCrossings = itSubroom.second->GetAllCrossings();
          for (Crossing* crossPtr : allCrossings) {
               if (!crossPtr->IsOpen()) {
                    _wall.emplace_back( Line( (Line) *crossPtr));

                    if (crossPtr->GetPoint1()._x < xMin) xMin = crossPtr->GetPoint1()._x;
                    if (crossPtr->GetPoint2()._x < xMin) xMin = crossPtr->GetPoint2()._x;
                    if (crossPtr->GetPoint1()._x > xMax) xMax = crossPtr->GetPoint1()._x;
                    if (crossPtr->GetPoint2()._x > xMax) xMax = crossPtr->GetPoint2()._x;

                    if (crossPtr->GetPoint1()._y < yMin) yMin = crossPtr->GetPoint1()._y;
                    if (crossPtr->GetPoint2()._y < yMin) yMin = crossPtr->GetPoint2()._y;
                    if (crossPtr->GetPoint1()._y > yMax) yMax = crossPtr->GetPoint1()._y;
                    if (crossPtr->GetPoint2()._y > yMax) yMax = crossPtr->GetPoint2()._y;
               }
          }
     }

     _subroomGrid = new RectGrid();
     _subroomGrid->setBoundaries(xMin, yMin, xMax, yMax);
     _subroomGrid->setSpacing(_subroomGridSpacing, _subroomGridSpacing);
     _subroomGrid->createGrid();

     // There was a case where _subroomGrid->GetKeyAtPoint(p) returns values >= nPoints (the check for includesPoints(p) was successful).
     // So we insert some additional entries to _subroomMap to avoid out_of_range errors. --f.mack
     // @todo Investigate why this is happening.
     long int max_index = _subroomGrid->GetnPoints() + _subroomGrid->GetiMax() + _subroomGrid->GetjMax();
     for (long int i = 0; i < max_index; ++i) {
          SubRoom* subroom = isInsideInit(i);
          _subroomMap.insert(std::make_pair(i, subroom));
     }

     //create Rect Grid
     _grid = new RectGrid();
     _grid->setBoundaries(xMin, yMin, xMax, yMax);
     _grid->setSpacing(hxArg, hyArg);
     _grid->createGrid();

     //create arrays
     //flag = new int[grid->GetnPoints()];                  //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, -7 = outside)
     _gcode = new int[_grid->GetnPoints()];
     _subroomUID = new int[_grid->GetnPoints()];
     _dist2Wall = new double[_grid->GetnPoints()];
     _speedInitial = new double[_grid->GetnPoints()];
     _modifiedspeed = new double[_grid->GetnPoints()];
     _densityspeed = new double[_grid->GetnPoints()];
     _cost = new double[_grid->GetnPoints()];
     _neggrad = new Point[_grid->GetnPoints()];
     _dirToWall = new Point[_grid->GetnPoints()];
     //trialfield = new Trial[grid->GetnPoints()];                 //created with other arrays, but not initialized yet

     _costmap.emplace(-1 , _cost);                         // enable default ff (closest exit)
     _neggradmap.emplace(-1, _neggrad);

     //init grid with -3 as unknown distance to any wall
     for(long int i = 0; i < _grid->GetnPoints(); ++i) {
          _dist2Wall[i] = -3.;
          _cost[i] = -2.;
          _gcode[i] = OUTSIDE;            //unknown
     }
     //drawLinesOnGrid(wall, dist2Wall, 0.);
     //drawLinesOnGrid(wall, cost, -7.);
     //drawLinesOnGrid(wall, gcode, WALL);
     //drawLinesOnGrid(exitsFromScope, gcode, OPEN_TRANSITION);

     drawLinesOnGrid(_wall, _dist2Wall, 0.);
     drawLinesOnGrid(_wall, _cost, -7.);
     drawLinesOnGrid(_wall, _gcode, WALL);
     drawLinesOnGrid(_exitsFromScope, _gcode, OPEN_TRANSITION);
}

void LocalFloorfieldViaFM::drawBlockerLines() {
     //std::vector<Line> exits(wall.begin(), wall.begin()+numOfExits);

     //grid handeling local vars:
     //long int iMax  = grid->GetiMax();

     long int iStart, iEnd;
     long int jStart, jEnd;
     long int iDot, jDot;
     long int key;
     long int deltaX, deltaY, deltaX1, deltaY1, px, py, xe, ye, i; //Bresenham Algorithm

     for(auto& line : _wall) {
          key = _grid->getKeyAtPoint(line.GetPoint1());
          iStart = (long) _grid->get_i_fromKey(key);
          jStart = (long) _grid->get_j_fromKey(key);

          key = _grid->getKeyAtPoint(line.GetPoint2());
          iEnd = (long) _grid->get_i_fromKey(key);
          jEnd = (long) _grid->get_j_fromKey(key);

          deltaX = (int) (iEnd - iStart);
          deltaY = (int) (jEnd - jStart);
          deltaX1 = abs( (int) (iEnd - iStart));
          deltaY1 = abs( (int) (jEnd - jStart));

          px = 2*deltaY1 - deltaX1;
          py = 2*deltaX1 - deltaY1;

          if(deltaY1<=deltaX1) {
               if(deltaX>=0) {
                    iDot = iStart;
                    jDot = jStart;
                    xe = iEnd;
               } else {
                    iDot = iEnd;
                    jDot = jEnd;
                    xe = iStart;
               }
               //crossOutOutsideNeighbors(jDot*iMax + iDot);
               for (i=0; iDot < xe; ++i) {
                    ++iDot;
                    if(px<0) {
                         px+=2*deltaY1;
                    } else {
                         if((deltaX<0 && deltaY<0) || (deltaX>0 && deltaY>0)) {
                              ++jDot;
                         } else {
                              --jDot;
                         }
                         px+=2*(deltaY1-deltaX1);
                    }
                    //crossOutOutsideNeighbors(jDot*iMax + iDot);
               }
          } else {
               if(deltaY>=0) {
                    iDot = iStart;
                    jDot = jStart;
                    ye = jEnd;
               } else {
                    iDot = iEnd;
                    jDot = jEnd;
                    ye = jStart;
               }
               //crossOutOutsideNeighbors(jDot*iMax + iDot);
               for(i=0; jDot<ye; ++i) {
                    ++jDot;
                    if (py<=0) {
                         py+=2*deltaX1;
                    } else {
                         if((deltaX<0 && deltaY<0) || (deltaX>0 && deltaY>0)) {
                              ++iDot;
                         } else {
                              --iDot;
                         }
                         py+=2*(deltaX1-deltaY1);
                    }
                   // crossOutOutsideNeighbors(jDot*iMax + iDot);
               }
          }
     } //loop over all walls

     for(auto& line : _exitsFromScope) {
          key = _grid->getKeyAtPoint(line.GetPoint1());
          iStart = (long) _grid->get_i_fromKey(key);
          jStart = (long) _grid->get_j_fromKey(key);

          key = _grid->getKeyAtPoint(line.GetPoint2());
          iEnd = (long) _grid->get_i_fromKey(key);
          jEnd = (long) _grid->get_j_fromKey(key);

          deltaX = (int) (iEnd - iStart);
          deltaY = (int) (jEnd - jStart);
          deltaX1 = abs( (int) (iEnd - iStart));
          deltaY1 = abs( (int) (jEnd - jStart));

          px = 2*deltaY1 - deltaX1;
          py = 2*deltaX1 - deltaY1;

          if(deltaY1<=deltaX1) {
               if(deltaX>=0) {
                    iDot = iStart;
                    jDot = jStart;
                    xe = iEnd;
               } else {
                    iDot = iEnd;
                    jDot = jEnd;
                    xe = iStart;
               }
               //crossOutOutsideNeighbors(jDot*iMax + iDot);
               for (i=0; iDot < xe; ++i) {
                    ++iDot;
                    if(px<0) {
                         px+=2*deltaY1;
                    } else {
                         if((deltaX<0 && deltaY<0) || (deltaX>0 && deltaY>0)) {
                              ++jDot;
                         } else {
                              --jDot;
                         }
                         px+=2*(deltaY1-deltaX1);
                    }
                    //crossOutOutsideNeighbors(jDot*iMax + iDot);
               }
          } else {
               if(deltaY>=0) {
                    iDot = iStart;
                    jDot = jStart;
                    ye = jEnd;
               } else {
                    iDot = iEnd;
                    jDot = jEnd;
                    ye = jStart;
               }
               //crossOutOutsideNeighbors(jDot*iMax + iDot);
               for(i=0; jDot<ye; ++i) {
                    ++jDot;
                    if (py<=0) {
                         py+=2*deltaX1;
                    } else {
                         if((deltaX<0 && deltaY<0) || (deltaX>0 && deltaY>0)) {
                              ++iDot;
                         } else {
                              --iDot;
                         }
                         py+=2*(deltaX1-deltaY1);
                    }
                   // crossOutOutsideNeighbors(jDot*iMax + iDot);
               }
          }
     } //loop over all exits

}
//@todo Arne: Is this function needed? delete?
//void LocalFloorfieldViaFM::crossOutOutsideNeighbors(const long int key){
//     directNeighbor dNeigh = grid->getNeighbors(key);
//     long int aux = -1;
//
//     const std::map<int, std::unique_ptr<SubRoom> >& subRoomMap = room->GetAllSubRooms();
//
//
//     aux = dNeigh.key[0];
////     if ((aux != -2) && (cost[aux] < -0.1) && (flag[aux] != -7) && (flag[aux] != -5)) { //aux is key of vaild girdpoint && gridpoint is not on exitline (exits have cost = 0 in prepareForDistance..())
////          Point trialP = grid->getPointFromKey(aux);               //^^ and gridpoint is not wall nor blockpoint
////          bool isInside = false;
////          for (int i = 0; i < subRoomMap.size(); ++i) {
////               auto subRoomIt = subRoomMap.begin();
////               std::advance(subRoomIt, i);
////               if ((*subRoomIt).second->IsInSubRoom(trialP)) {
////                    isInside = true;
////               }
////          }
//          if (!isInside(aux)) {
//               flag[aux] = -5;
//               dist2Wall[aux] = 0.; //set dist2Wall == 0 to save this points from updates in FloorfieldViaFM::clearAndPrepareForFloorfieldReCalc
//               speedInitial[aux] = .001;
//               cost[aux]         = -8.;
//          }
////     }
//     aux = dNeigh.key[1];
////     if ((aux != -2) && (cost[aux] < 0.) && (flag[aux] != -7) && (flag[aux] != -5)) {
////          Point trialP = grid->getPointFromKey(aux);
////          bool isInside = false;
////          for (int i = 0; i < subRoomMap.size(); ++i) {
////               auto subRoomIt = subRoomMap.begin();
////               std::advance(subRoomIt, i);
////               if ((*subRoomIt).second->IsInSubRoom(trialP)) {
////                    isInside = true;
////               }
////          }
////          if (!isInside) {
////               flag[aux] = -5;
////               dist2Wall[aux] = 0.;
////               speedInitial[aux] = .001;
////               cost[aux]         = -8.;
////          }
////     }
//     aux = dNeigh.key[2];
////     if ((aux != -2) && (cost[aux] < 0.) && (flag[aux] != -7) && (flag[aux] != -5)) {
////          Point trialP = grid->getPointFromKey(aux);
////          bool isInside = false;
////          for (int i = 0; i < subRoomMap.size(); ++i) {
////               auto subRoomIt = subRoomMap.begin();
////               std::advance(subRoomIt, i);
////               if ((*subRoomIt).second->IsInSubRoom(trialP)) {
////                    isInside = true;
////               }
////          }
////          if (!isInside) {
////               flag[aux] = -5;
////               dist2Wall[aux] = 0.;
////               speedInitial[aux] = .001;
////               cost[aux]         = -8.;
////          }
////     }
//     aux = dNeigh.key[3];
////     if ((aux != -2) && (cost[aux] < 0.) && (flag[aux] != -7) && (flag[aux] != -5)) {
////          Point trialP = grid->getPointFromKey(aux);
////          bool isInside = false;
////          for (int i = 0; i < subRoomMap.size(); ++i) {
////               auto subRoomIt = subRoomMap.begin();
////               std::advance(subRoomIt, i);
////               if ((*subRoomIt).second->IsInSubRoom(trialP)) {
////                    isInside = true;
////               }
////          }
////          if (!isInside) {
////               flag[aux] = -5;
////               dist2Wall[aux] = 0.;
////               speedInitial[aux] = .001;
////               cost[aux]         = -8.;
////          }
////     }
//}

SubRoom* LocalFloorfieldViaFM::isInsideInit(const long int key) {
     Point probe = _subroomGrid->getPointFromKey(key);

     const std::map<int, std::shared_ptr<SubRoom>>& subRoomMap = _room->GetAllSubRooms();

     for (auto& subRoomPair : subRoomMap) {

          SubRoom* subRoomPtr = subRoomPair.second.get();

          if (subRoomPtr->IsInSubRoom(probe)) {
               return subRoomPtr;
          }
     }

     return nullptr;
}

int LocalFloorfieldViaFM::isInside(const long int key) {
     Point probe = _grid->getPointFromKey(key);

     // rounds downwards to the next number that is a multiple of _subroomGridSpacing, considering an offset of xMin/yMin
     double lowerX = _subroomGridSpacing * floor((probe._x - _subroomGrid->GetxMin()) / _subroomGridSpacing) + _subroomGrid->GetxMin();
     double lowerY = _subroomGridSpacing * floor((probe._y - _subroomGrid->GetyMin()) / _subroomGridSpacing) + _subroomGrid->GetyMin();

     Point subroomPoint = Point(lowerX, lowerY);
     SubRoom* sub1 = _subroomGrid->includesPoint(subroomPoint) ? _subroomMap.at(_subroomGrid->getKeyAtPoint(subroomPoint)) : nullptr;
     if (sub1 && sub1->IsInSubRoom(probe)) return sub1->GetUID();

     subroomPoint._x += _subroomGridSpacing;
     SubRoom* sub2 = _subroomGrid->includesPoint(subroomPoint) ? _subroomMap.at(_subroomGrid->getKeyAtPoint(subroomPoint)) : nullptr;
     if (sub2 && sub2 != sub1 && sub2->IsInSubRoom(probe)) return sub2->GetUID();

     subroomPoint._y += _subroomGridSpacing;
     SubRoom* sub3 = _subroomGrid->includesPoint(subroomPoint) ? _subroomMap.at(_subroomGrid->getKeyAtPoint(subroomPoint)) : nullptr;
     if (sub3 && sub3 != sub1 && sub3 != sub2 && sub3->IsInSubRoom(probe)) return sub3->GetUID();

     subroomPoint._x -= _subroomGridSpacing;
     SubRoom* sub4 = _subroomGrid->includesPoint(subroomPoint) ? _subroomMap.at(_subroomGrid->getKeyAtPoint(subroomPoint)) : nullptr;
     if (sub4 && sub4 != sub1 && sub4 != sub2 && sub4 != sub3 && sub4->IsInSubRoom(probe)) return sub4->GetUID();

     const std::map<int, std::shared_ptr<SubRoom>>& subRoomMap = _room->GetAllSubRooms();

     for (auto& subRoomPair : subRoomMap) {

          SubRoom* subRoomPtr = subRoomPair.second.get();
          if (subRoomPtr == sub1 || subRoomPtr == sub2 || subRoomPtr == sub3 || subRoomPtr == sub4) continue;

          if (subRoomPtr->IsInSubRoom(probe)) {
               return subRoomPtr->GetUID();
          }
     }

     return 0;
}

SubLocalFloorfieldViaFM::SubLocalFloorfieldViaFM(){};
SubLocalFloorfieldViaFM::SubLocalFloorfieldViaFM(const SubRoom* const roomArg,
      const Building* buildingArg,
      const double hxArg, const double hyArg,
      const double wallAvoidDistance, const bool useDistancefield) {
     //ctor
     _threshold = -1; //negative value means: ignore threshold
     _threshold = wallAvoidDistance;
     _building = buildingArg;
     _subroom = roomArg;

     if (hxArg != hyArg) std::cerr << "ERROR: hx != hy <=========";
     //parse building and create list of walls/obstacles (find xmin xmax, ymin, ymax, and add border?)
     //Log->Write("INFO: \tStart Parsing: Room %d" , roomArg->GetUID());
     parseRoom(roomArg, hxArg, hyArg);
     //Log->Write("INFO: \tFinished Parsing: Room %d" , roomArg->GetUID());
     //testoutput("AALineScan.vtk", "AALineScan.txt", dist2Wall);

     prepareForDistanceFieldCalculation(false);
     //here we need to draw blocker lines @todo: ar.graf
     //Log->Write("INFO: \tGrid initialized: Walls");

     calculateDistanceField(-1.); //negative threshold is ignored, so all distances get calculated. this is important since distances is used for slowdown/redirect
     //Log->Write("INFO: \tGrid initialized: Walldistances");

     setSpeed(useDistancefield); //use distance2Wall
     //Log->Write("INFO: \tGrid initialized: Speed");
     calculateFloorfield(_exitsFromScope, _cost, _neggrad);
};

void SubLocalFloorfieldViaFM::getDirectionToDestination(Pedestrian* ped,
      Point& direction)
{
     FloorfieldViaFM::getDirectionToDestination(ped, direction);
     return;
}

void SubLocalFloorfieldViaFM::getDirectionToGoalID(const int goalID){
     std::cerr << "invalid call to SubLocalFloorfieldViaFM::getDirectionToGoalID with goalID: " << goalID << std::endl;
};


void SubLocalFloorfieldViaFM::parseRoom(const SubRoom* const roomArg,
      const double hxArg, const double hyArg)
{
     _subroom = roomArg;
     //init min/max before parsing
     double xMin = DBL_MAX;
     double xMax = -DBL_MAX;
     double yMin = xMin;
     double yMax = xMax;
     _costmap.clear();
     _neggradmap.clear();

     //create a list of walls
     //add all transition and put open doors at the beginning of "wall"
     std::map<int, Transition*> allTransitions;

     for (auto itTrans : _subroom->GetAllTransitions()) {
          if (!allTransitions.count(itTrans->GetUniqueID())) {
               allTransitions[itTrans->GetUniqueID()] = &(*itTrans);
               if (itTrans->IsOpen()) {
                    _exitsFromScope.emplace_back(Line((Line) *itTrans));
                    _costmap.emplace(itTrans->GetUniqueID(), nullptr);
                    _neggradmap.emplace(itTrans->GetUniqueID(), nullptr);
               }
          }
     }

     for (auto itCross : _subroom->GetAllCrossings()) {
          if (itCross->IsOpen()) {
               _exitsFromScope.emplace_back(Line((Line) *itCross));
               _costmap.emplace(itCross->GetUniqueID(), nullptr);
               _neggradmap.emplace(itCross->GetUniqueID(), nullptr);
          } else {
               _wall.emplace_back(Line( (Line) *itCross));
          }
     }

     _numOfExits = _exitsFromScope.size();
     //put closed doors next, they are considered as walls later (index >= numOfExits)
     for (auto& trans : allTransitions) {
          if (!trans.second->IsOpen()) {
               _wall.emplace_back(Line ( (Line) *(trans.second)));
          }
     }


     std::vector<Obstacle*> allObstacles = _subroom->GetAllObstacles();
     for (std::vector<Obstacle*>::iterator itObstacles = allObstacles.begin(); itObstacles != allObstacles.end(); ++itObstacles) {

          std::vector<Wall> allObsWalls = (*itObstacles)->GetAllWalls();
          for (std::vector<Wall>::iterator itObsWall = allObsWalls.begin(); itObsWall != allObsWalls.end(); ++itObsWall) {
               _wall.emplace_back(Line( (Line) *itObsWall));
               // xMin xMax
               if ((*itObsWall).GetPoint1()._x < xMin) xMin = (*itObsWall).GetPoint1()._x;
               if ((*itObsWall).GetPoint2()._x < xMin) xMin = (*itObsWall).GetPoint2()._x;
               if ((*itObsWall).GetPoint1()._x > xMax) xMax = (*itObsWall).GetPoint1()._x;
               if ((*itObsWall).GetPoint2()._x > xMax) xMax = (*itObsWall).GetPoint2()._x;
               // yMin yMax
               if ((*itObsWall).GetPoint1()._y < yMin) yMin = (*itObsWall).GetPoint1()._y;
               if ((*itObsWall).GetPoint2()._y < yMin) yMin = (*itObsWall).GetPoint2()._y;
               if ((*itObsWall).GetPoint1()._y > yMax) yMax = (*itObsWall).GetPoint1()._y;
               if ((*itObsWall).GetPoint2()._y > yMax) yMax = (*itObsWall).GetPoint2()._y;
          }
     }

     std::vector<Wall> allWalls = _subroom->GetAllWalls();
     for (std::vector<Wall>::iterator itWall = allWalls.begin(); itWall != allWalls.end(); ++itWall) {
          _wall.emplace_back( Line( (Line) *itWall));
          // xMin xMax
          if ((*itWall).GetPoint1()._x < xMin) xMin = (*itWall).GetPoint1()._x;
          if ((*itWall).GetPoint2()._x < xMin) xMin = (*itWall).GetPoint2()._x;
          if ((*itWall).GetPoint1()._x > xMax) xMax = (*itWall).GetPoint1()._x;
          if ((*itWall).GetPoint2()._x > xMax) xMax = (*itWall).GetPoint2()._x;
          // yMin yMax
          if ((*itWall).GetPoint1()._y < yMin) yMin = (*itWall).GetPoint1()._y;
          if ((*itWall).GetPoint2()._y < yMin) yMin = (*itWall).GetPoint2()._y;
          if ((*itWall).GetPoint1()._y > yMax) yMax = (*itWall).GetPoint1()._y;
          if ((*itWall).GetPoint2()._y > yMax) yMax = (*itWall).GetPoint2()._y;
     }

     const vector<Crossing*>& allCrossings = _subroom->GetAllCrossings();
     for (Crossing* crossPtr : allCrossings) {
          if (!crossPtr->IsOpen()) {
               _wall.emplace_back( Line( (Line) *crossPtr));

               if (crossPtr->GetPoint1()._x < xMin) xMin = crossPtr->GetPoint1()._x;
               if (crossPtr->GetPoint2()._x < xMin) xMin = crossPtr->GetPoint2()._x;
               if (crossPtr->GetPoint1()._x > xMax) xMax = crossPtr->GetPoint1()._x;
               if (crossPtr->GetPoint2()._x > xMax) xMax = crossPtr->GetPoint2()._x;

               if (crossPtr->GetPoint1()._y < yMin) yMin = crossPtr->GetPoint1()._y;
               if (crossPtr->GetPoint2()._y < yMin) yMin = crossPtr->GetPoint2()._y;
               if (crossPtr->GetPoint1()._y > yMax) yMax = crossPtr->GetPoint1()._y;
               if (crossPtr->GetPoint2()._y > yMax) yMax = crossPtr->GetPoint2()._y;
          }
     }


     //create Rect Grid
     _grid = new RectGrid();
     _grid->setBoundaries(xMin, yMin, xMax, yMax);
     _grid->setSpacing(hxArg, hyArg);
     _grid->createGrid();

     //create arrays
     _gcode = new int[_grid->GetnPoints()];                  //flag:( 0 = unknown, 1 = singel, 2 = double, 3 = final, -7 = outside)
     _subroomUID = new int[_grid->GetnPoints()];
     _dist2Wall = new double[_grid->GetnPoints()];
     _speedInitial = new double[_grid->GetnPoints()];
     _modifiedspeed = new double[_grid->GetnPoints()];
     _densityspeed = new double[_grid->GetnPoints()];
     _cost = new double[_grid->GetnPoints()];
     _neggrad = new Point[_grid->GetnPoints()];
     _dirToWall = new Point[_grid->GetnPoints()];
     //trialfield = new Trial[grid->GetnPoints()];                 //created with other arrays, but not initialized yet

     _costmap.emplace(-1 , _cost);                         // enable default ff (closest exit)
     _neggradmap.emplace(-1, _neggrad);

     //init grid with -3 as unknown distance to any wall
     for(long int i = 0; i < _grid->GetnPoints(); ++i) {
          _dist2Wall[i] = -3.;
          _cost[i] = -2.;
          _gcode[i] = OUTSIDE;
     }
     //drawLinesOnGrid(wall, gcode, WALL);
     //drawLinesOnGrid(exitsFromScope, gcode, OPEN_TRANSITION);
     drawLinesOnGrid(_wall, _dist2Wall, 0.);
     drawLinesOnGrid(_wall, _cost, -7.);
     drawLinesOnGrid(_wall, _gcode, WALL);
     drawLinesOnGrid(_exitsFromScope, _gcode, OPEN_TRANSITION);
}

int SubLocalFloorfieldViaFM::isInside(const long int key) {
     Point probe = _grid->getPointFromKey(key);
     return _subroom->IsInSubRoom(probe)?_subroom->GetUID():0;
}