/*
 * VoronoiPositionGenerator.cpp
 *
 *  Created on: Sep 2, 2015
 *      Author: gsp1502
 */

#include "VoronoiPositionGenerator.h"
//check if all includes are necessary
#include "../pedestrian/AgentsSourcesManager.h"
#include "../pedestrian/Pedestrian.h"
//#include "../pedestrian/StartDistribution.h"
//#include "../pedestrian/PedDistributor.h"
//#include "../pedestrian/AgentsSource.h"
//#include "../geometry/Building.h"
//#include "../geometry/Point.h"

//#include "../mpi/LCGrid.h"
//#include <iostream>
#include <thread>
//#include <chrono>


//#include "../geometry/SubRoom.h"
//#include <stdlib.h>
//#include <time.h>
//#include <string>
//#include <random>

#include "boost/polygon/voronoi.hpp"
using boost::polygon::voronoi_builder;
using boost::polygon::voronoi_diagram;
using boost::polygon::x;
using boost::polygon::y;
using boost::polygon::low;
using boost::polygon::high;


//wrapping the boost objects (just the point object)
namespace boost {
namespace polygon {
template <>
struct geometry_concept<Point> {
     typedef point_concept type;
};

template <>
struct point_traits<Point> {
     typedef int coordinate_type;

     static inline coordinate_type get(
               const Point& point, orientation_2d orient) {
          return (orient == HORIZONTAL) ? point._x : point._y;
     }
};
}  // polygon
}  // boost


using namespace std;


//functions
//TODO: refactor the function
bool IsEnoughInSubroom( SubRoom* subroom, Point& pt, double radius )
{
     for (const auto& wall: subroom->GetAllWalls())
          if(wall.DistTo(pt)<radius)
               return false;

     for(const auto& trans: subroom->GetAllTransitions() )
    	 if ( trans->DistTo(pt) < radius + 0.1 )
    		 return false;

     for( const auto& cross: subroom->GetAllCrossings() )
    	 if( cross->DistTo(pt) < radius + 0.1 )
    		 return false;

     return true;
}

bool ComputeBestPositionVoronoiBoost(AgentsSource* src, std::vector<Pedestrian*>& peds,
         Building* building)
{
    bool return_value = true;
    auto dist = src->GetStartDistribution();
    int roomID = dist->GetRoomId();
    int subroomID = dist->GetSubroomID();
    // std::string caption = (building->GetRoom( roomID ))->GetCaption();

    std::vector<Pedestrian*> existing_peds;
    std::vector<Pedestrian*> peds_without_place;
    building->GetPedestrians(roomID, subroomID, existing_peds);

    double radius = 0.3; //radius of a person, 0.3 is just some number(needed for the fake_peds bellow), will be changed afterwards

    SubRoom* subroom = building->GetRoom( roomID )->GetSubRoom(subroomID);

    double factor = 100;  //factor for conversion to integer for the boost voronoi

    std::vector<Point> fake_peds;
    Point temp(0,0);
    //fake_peds will be the positions of "fake" pedestrians, multiplied by factor and converted to int
    for (auto vert: subroom->GetPolygon() ) //room vertices
    {
          const Point& center_pos = subroom->GetCentroid();
          temp._x = ( center_pos._x-vert._x );
          temp._y = ( center_pos._y-vert._y );
          temp = temp/sqrt(temp.NormSquare());
          temp = temp*(radius*1.4);  //now the norm of the vector is ~r*sqrt(2), pointing to the center
          temp = temp + vert;
          temp._x = (int)(temp._x*factor);
          temp._y = (int)(temp._y*factor);
          fake_peds.push_back( temp );
    }

    std::vector<Pedestrian*>::iterator iter_ped;
    for (iter_ped = peds.begin(); iter_ped != peds.end(); )
    {
         Pedestrian* ped = *iter_ped;
         radius = ped->GetEllipse().GetBmax(); //max radius of the current pedestrian

         if(existing_peds.size() == 0 )
         {
       	   const Point& center_pos = subroom->GetCentroid();

              double x_coor = 3 * ( (double)rand() / (double)RAND_MAX ) - 1.5;
              double y_coor = 3 * ( (double)rand() / (double)RAND_MAX ) - 1.5;
              Point random_pos(x_coor, y_coor);
              Point new_pos = center_pos + random_pos;
              //this could be better, but needs to work with any polygon - random point inside a polygon?
              if ( subroom->IsInSubRoom( new_pos ) )
              {
                   if( IsEnoughInSubroom(subroom, new_pos, radius ) )
                   {
                        ped->SetPos(center_pos + random_pos, true);
                   }
              }
              else
              {
                    ped->SetPos(center_pos, true);
              }
              
              Point v;
              if (ped->GetExitLine()) {
                    v = (ped->GetExitLine()->ShortestPoint(ped->GetPos())- ped->GetPos()).Normalized();
              } else {
                    v = Point(0., 0.);
              }
              //double speed=ped->GetV0Norm();
              double speed = ped->GetEllipse().GetV0(); //@todo: some peds do not have a navline. This should not be accepted.
              v=v*speed;
              ped->SetV(v);
              existing_peds.push_back(ped);

         }//0

         else //more than one pedestrian
         {
       	   //it would be better to maybe have a mapping between discrete_positions and pointers to the pedestrians
       	   //then there would be no need to remember the velocities_vector and goal_vector
     	       std::vector<Point> discrete_positions;
              std::vector<Point> velocities_vector;
              std::vector<int> goal_vector;
              Point tmp(0,0);
              Point v(0,0);
              double no = 0;

              //points from double to integer
              for (const auto& eped: existing_peds)
              {
                   const Point& pos = eped->GetPos();
                   tmp._x = (int)( pos._x*factor );
                   tmp._y = (int)( pos._y*factor );
                   discrete_positions.push_back( tmp );
                   velocities_vector.push_back( eped->GetV() );
                   goal_vector.push_back( eped->GetFinalDestination() );

                   //calculating the mean, using it for the fake pedestrians
                   v = v + eped->GetV();
                   no++;
              }

              //TODO: dividing by 0 when existing_peds is empty
              // -------> existing_peds is not empty because of the if statement
              // sum up the weighted velocity in the loop
              v = v/no; //this is the mean of all velocities

              //adding fake people to the vector for constructing voronoi diagram
              for (unsigned int i=0; i<subroom->GetPolygon().size(); i++ )
              {
                   discrete_positions.push_back( fake_peds[i] );
                   velocities_vector.push_back( v );
                   goal_vector.push_back( -10 );
              }

              //constructing the diagram
              voronoi_diagram<double> vd;
              construct_voronoi(discrete_positions.begin(), discrete_positions.end(), &vd);

              voronoi_diagram<double>::const_vertex_iterator chosen_it = vd.vertices().begin();
              double dis = 0;
              VoronoiBestVertexRandMax(discrete_positions, vd, subroom, factor, chosen_it, dis, radius );

              if( dis > 4*radius*factor*radius*factor)// be careful with the factor!! radius*factor, 2,3,4?
              {
                   Point pos( chosen_it->x()/factor, chosen_it->y()/factor ); //check!
                   ped->SetPos(pos , true);
                   VoronoiAdjustVelocityNeighbour(chosen_it, ped, velocities_vector, goal_vector);

                   // proceed to the next pedestrian
                   existing_peds.push_back(ped);
                   ++iter_ped;

              }
              else
              {
                   //reject the pedestrian:
                   return_value = false;
                   peds_without_place.push_back(*iter_ped); //Put in a different queue, they will be put back in the source.
                   iter_ped=peds.erase(iter_ped); // remove from the initial vector since it should only contain the pedestrians that could find a place
              }

              /*else //try with the maximum distance, don't need this if already using the VoronoiBestVertexMax function
				{
					VoronoiBestVertexMax(discrete_positions, vd, subroom, factor, chosen_it, dis );
					if( dis > radius*factor*radius*factor)// be careful with the factor!! radius*factor
					{
						Point pos( chosen_it->x()/factor, chosen_it->y()/factor ); //check!
						ped->SetPos(pos , true);
						VoronoiAdjustVelocityNeighbour( vd, chosen_it, ped, velocities_vector );
					}
					else
					{
						return_value = false;
						//reject the pedestrian
					}
				}*/
         }// >0


    }//for loop


    //maybe not all pedestrians could find a place, requeue them in the source
    if(peds_without_place.size()>0)
         src->AddAgentsToPool(peds_without_place);

    return return_value;
}

//gives an agent the mean velocity of his voronoi-neighbors
void VoronoiAdjustVelocityNeighbour(voronoi_diagram<double>::const_vertex_iterator& chosen_it, Pedestrian* ped,
        const std::vector<Point>& velocities_vector, const std::vector<int>& goal_vector)
{
     //finding the neighbors (nearest pedestrians) of the chosen vertex
     const voronoi_diagram<double>::vertex_type &vertex = *chosen_it;
     const voronoi_diagram<double>::edge_type *edge = vertex.incident_edge();
     double no1=0,no2=0;
     double backup_speed = 0;
     //std::size_t index;
     Point v(0,0);
     if(ped->GetExitLine() != nullptr)
          v = (ped->GetExitLine()->ShortestPoint(ped->GetPos())- ped->GetPos()).Normalized(); //the direction
     else
     {
          int gotRoute = ped->FindRoute();
          if(gotRoute < 0) printf("\nWARNING: source agent %d can not get exit\n", ped->GetID());
     }
     double speed = 0;
     do
     {
          std::size_t index = ( edge->cell() )->source_index();
          if( ped->GetFinalDestination() == goal_vector[index]  )
          {
        	  no1++;
        	  speed += velocities_vector[index].Norm();
          }
          else
          {
        	  no2++;
        	  backup_speed += velocities_vector[index].Norm();
          }
          edge = edge->rot_next();
     } while (edge != vertex.incident_edge());

     if(no1)
    	 speed = speed/no1;
     else
    	 speed = backup_speed/(no2*3.0); //just some small speed

     v = v*speed;
     ped->SetV(v);

}


//gives the voronoi vertex with max distance
//void VoronoiBestVertexMax(const std::vector<Point>& discrete_positions, const voronoi_diagram<double>& vd,
//        SubRoom* subroom, double factor, voronoi_diagram<double>::const_vertex_iterator& max_it, double& max_dis,
//        double radius)
//{
//     double dis = 0;
//     double score;
//     double max_score = -100; //calculated using distance and considering the goal
//
//
//
//     for (auto it = vd.vertices().begin(); it != vd.vertices().end(); ++it)
//     {
//          Point vert_pos( it->x()/factor, it->y()/factor );
//          if( subroom->IsInSubRoom(vert_pos) )
//               if( IsEnoughInSubroom( subroom, vert_pos, radius ) )
//               {
//                    const voronoi_diagram<double>::vertex_type &vertex = *it;
//                    const voronoi_diagram<double>::edge_type *edge = vertex.incident_edge();
//
//                    std::size_t index = ( edge->cell() )->source_index();
//                    Point p = discrete_positions[index];
//
//                    dis = ( p._x - it->x() )*( p._x - it->x() )  + ( p._y - it->y() )*( p._y - it->y() )  ;
//
//                    score = dis;
//
//
//                    //constructing the checking line
///*
//                    Point p2 = (ped->GetExitLine()->ShortestPoint(vert_pos)-vert_pos).Normalized(); //problem: ped does not have a position
//                    p2 = p2 + p2; //looking 2m in front
//                    Line check_line(vert_pos, vert_pos + p2);  //this is the first 2m of exit line
//
//                    do
//                    {
//                    	//do something
//                    	if( goal_vector[index]!=-3 &&  goal_vector[index]!=ped->GetFinalDestination() ) //
//                    		if( check_line.IntersectionWithCircle(p,1.0) )    //0.7 because the radius is around 0.3
//                    		{
//                    			score -= 100;
//                    			break;
//                    		}
//
//
//                    	//change edge
//                    	edge = edge->rot_next();
//                    	index = ( edge->cell() )->source_index();
//                    	p = discrete_positions[index]/factor;
//
//                    } while( edge != vertex.incident_edge() );
//*/
//                    if(score > max_score)
//                    {
//                         max_score =score;
//                    	 max_dis = dis;
//                         max_it = it;
//                    }
//               }
//     }
//     //at the end, max_it is the choosen vertex, or the first vertex - max_dis=0 assures that this position will not be taken
//}

//gives random voronoi vertex but with weights proportional to distances^2
//in case you want proportional to distance^4 just change two lines commented as HERE
void VoronoiBestVertexRandMax (const std::vector<Point>& discrete_positions, const voronoi_diagram<double>& vd, SubRoom* subroom,
          double factor, voronoi_diagram<double>::const_vertex_iterator& chosen_it, double& dis	, double radius)
{
     std::vector< voronoi_diagram<double>::const_vertex_iterator > possible_vertices;
     vector<double> partial_sums;
     unsigned long size=0;

     for (auto it = vd.vertices().begin(); it != vd.vertices().end(); ++it)
     {
          Point vert_pos = Point( it->x()/factor, it->y()/factor );
          if( subroom->IsInSubRoom( vert_pos ) )
               if( IsEnoughInSubroom(subroom, vert_pos,radius) )
               {
                    const voronoi_diagram<double>::vertex_type &vertex = *it;
                    const voronoi_diagram<double>::edge_type *edge = vertex.incident_edge();

                    std::size_t index = ( edge->cell() )->source_index();
                    Point p = discrete_positions[index];

                    dis = ( p._x - it->x() )*( p._x - it->x() )   + ( p._y - it->y() )*( p._y - it->y() )  ;

                    possible_vertices.push_back( it );
                    partial_sums.push_back( dis ); // HERE

                    size = partial_sums.size();
                    if( size > 1 )
                    {
                         partial_sums[ size - 1 ] += partial_sums[ size - 2 ];
                    }
               }
     }
     //now we have the vector of possible vertices and weights and we can choose one randomly

     double lower_bound = 0;
     double upper_bound = partial_sums[size-1];
     std::uniform_real_distribution<double> unif(lower_bound,upper_bound);
     std::default_random_engine re;
     double a_random_double = unif(re);

     for (unsigned int i=0; i<size; i++)
     {
          if ( partial_sums[i] >= a_random_double )
          {
               //this is the chosen index
               chosen_it = possible_vertices[i];
               dis = partial_sums[i];
               if( i > 1 )
                    dis -= partial_sums[i-1];
               break;
          }
     }
     //dis = sqrt(dis); //HERE
}

//gives a random voronoi vertex
//void VoronoiBestVertexRand (const std::vector<Point>& discrete_positions, const voronoi_diagram<double>& vd, SubRoom* subroom,
//          double factor, voronoi_diagram<double>::const_vertex_iterator& chosen_it, double& dis, double radius	)
//{
//     std::vector< voronoi_diagram<double>::const_vertex_iterator > possible_vertices;
//     std::vector<double> distances;
//
//     for (auto it = vd.vertices().begin(); it != vd.vertices().end(); ++it)
//     {
//          Point vert_pos = Point( it->x()/factor, it->y()/factor );
//          if( subroom->IsInSubRoom(vert_pos) )
//               if( IsEnoughInSubroom(subroom, vert_pos, radius) )
//               {
//                    const voronoi_diagram<double>::vertex_type &vertex = *it;
//                    const voronoi_diagram<double>::edge_type *edge = vertex.incident_edge();
//
//                    std::size_t index = ( edge->cell() )->source_index();
//                    Point p = discrete_positions[index];
//
//                    dis = ( p._x - it->x() )*( p._x - it->x() )   + ( p._y - it->y() )*( p._y - it->y() )  ;
//
//                    possible_vertices.push_back( it );
//                    distances.push_back( dis );
//               }
//     }
//     //now we have all the possible vertices and their distances and we can choose one randomly
//     //TODO: get the seed from the simulation/argumentparser
//     //srand (time(NULL));
//     unsigned int i = rand() % possible_vertices.size();
//     chosen_it = possible_vertices[i];
//     dis = distances[i];
//
//}


