#ifndef WAYID_DESCRIPTOR_H_
#define WAYID_DESCRIPTOR_H_

#include "BaseDescriptor.h"

#include <boost/foreach.hpp>

template<class DataFacadeT> class GenRobDescriptor : public BaseDescriptor<DataFacadeT> {
private:
    DescriptorConfig config;
    FixedPointCoordinate current;

    std::string tmp;
public:
    void SetConfig(const DescriptorConfig & c) { config = c; }

    //TODO: reorder parameters
    void Run(
        http::Reply & reply,
        const RawRouteData &rawRoute,
        PhantomNodes &phantomNodes,
        const DataFacadeT * facade
    ) {
        reply.content.push_back("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        reply.content.push_back("<genrob>\n");

        bool found_route =  (rawRoute.lengthOfShortestPath != INT_MAX) &&
                            (rawRoute.computedShortestPath.size()         );
        if( found_route ) {

            reply.content.push_back("<route>\n");

            std::string startNodeLon, startNodeLat;
            std::string targetNodeLon, targetNodeLat;

            convertInternalLatLonToString(phantomNodes.startPhantom.location.lon, startNodeLon);
            convertInternalLatLonToString(phantomNodes.startPhantom.location.lat, startNodeLat);
            convertInternalLatLonToString(phantomNodes.targetPhantom.location.lon, targetNodeLon);
            convertInternalLatLonToString(phantomNodes.targetPhantom.location.lat, targetNodeLat);

            std::stringstream sstr_start;
            sstr_start << "\t<waypoint><lon>" << startNodeLon << "</lon><lat>" << startNodeLat << "</lat></waypoint>\n";
            reply.content.push_back(sstr_start.str());

            BOOST_FOREACH(
                const _PathData & pathData,
                rawRoute.computedShortestPath
            ) {
                std::string lonString, latString;

                FixedPointCoordinate current = facade->GetCoordinateOfNode(pathData.node);
                convertInternalLatLonToString(current.lon, lonString);
                convertInternalLatLonToString(current.lat, latString);

                std::stringstream sstr;
                sstr << "\t<waypoint><lon>" << lonString << "</lon><lat>" << latString << "</lat></waypoint>\n";
                reply.content.push_back(sstr.str());
            }

            std::stringstream sstr_target;
            sstr_target << "\t<waypoint><lon>" << targetNodeLon << "</lon><lat>" << targetNodeLat << "</lat></waypoint>\n";
            reply.content.push_back(sstr_target.str());

            reply.content.push_back("</route>\n");
            reply.content.push_back("<wayids>\n");
            long lastID = 0;
            BOOST_FOREACH(
                const _PathData & pathData,
                rawRoute.computedShortestPath
            ) {
				if (pathData.nameID != lastID) {
					std::stringstream sstr;
                    sstr << "\t<id>" << pathData.nameID << "</id>\n";
					reply.content.push_back(sstr.str());
					lastID = pathData.nameID;
				}
				lastID = pathData.nameID;
            }

            reply.content.push_back("</wayids>\n");
        }

        reply.content.push_back("</genrob>");
    }
};
#endif // GENROB_DESCRIPTOR_H_
