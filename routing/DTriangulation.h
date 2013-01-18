/**
 * @file    DTriangulation
 * @author  Ulrich Kemloh <kemlohulrich@gmail.com>
 * \date Created on: DNov 30, 2012
 *
 * @section LICENSE
 * This file is part of JuPedSim.
 *
 * JuPedSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * JuPedSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JuPedSim. If not, see <http://www.gnu.org/licenses/>.
 *
 * @section DESCRIPTION
 * Perform the Delauney triangulation of a polygon with holes.
 *
 *
 */

#ifndef DTRIANGULATION_H_
#define DTRIANGULATION_H_

#include <cstdlib>
#include <time.h>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <vector>

#include "../geometry/Point.h"
#include "../poly2tri/poly2tri.h"


class DTriangulation {
public:
	DTriangulation();

	virtual ~DTriangulation();

	/**
	 * Triangulate the specified domain
	 * \see SetOuterPolygone
	 * \see AddHole
	 */
	void Triangulate();

	/**
	 * @return the triangles resulting from the triangulation
	 */
	std::vector<p2t::Triangle*>  GetTriangles(){
		return _cdt->GetTriangles();
	}

	/**
	 * Set the boundaries of the domain
	 * @param outerConstraints
	 */
	void SetOuterPolygone(std::vector<Point>  outerConstraints);

	/**
	 * Add a new hole
	 * A domain can contains holes.
	 * They should fully be inside the domain.
	 */
	void AddHole(std::vector<Point>  hole);

	//templates for freing and clearing a vector of pointers
	template <class C> void FreeClear( C & cntr ) {
		for ( typename C::iterator it = cntr.begin();
				it != cntr.end(); ++it ) {
			delete * it;
		}
		cntr.clear();
	}

private:
	std::vector< std::vector<p2t::Point*> > _holesPolylines;
	std::vector<p2t::Point*> _outerConstraintsPolyline;
	p2t::CDT* _cdt;

};

#endif /* DTRIANGULATION_H_ */