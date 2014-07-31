#ifndef WAYID_DESCRIPTOR_H_
#define WAYID_DESCRIPTOR_H_

#include "BaseDescriptor.h"

#include <boost/foreach.hpp>

template<class DataFacadeT> class WayIdDescriptor : public BaseDescriptor<DataFacadeT> {
private:
    DescriptorConfig config;
    FixedPointCoordinate current;
    DataFacadeT * facade;

public:
    WayIdDescriptor(DataFacadeT *facade) : facade(facade) {}

    void SetConfig(const DescriptorConfig & c) { config = c; }

    void AddRoutePoint(const FixedPointCoordinate & coordinate, std::vector<char> & output)
    {
        const std::string route_point_head   = "  <waypoint><lon>";
        const std::string route_point_middle = "</lon><lat>";
        const std::string route_point_tail   = "</lat></waypoint>\n";

        std::string tmp;

        FixedPointCoordinate::convertInternalLatLonToString(coordinate.lat, tmp);
        output.insert(output.end(), route_point_head.begin(), route_point_head.end());
        output.insert(output.end(), tmp.begin(), tmp.end());

        FixedPointCoordinate::convertInternalLatLonToString(coordinate.lon, tmp);
        output.insert(output.end(), route_point_middle.begin(), route_point_middle.end());
        output.insert(output.end(), tmp.begin(), tmp.end());
        output.insert(output.end(), route_point_tail.begin(), route_point_tail.end());
    }

    void Run(const RawRouteData &raw_route, http::Reply &reply)
    {
        std::string tmp;

        std::string header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<result>\n";

        reply.content.insert(reply.content.end(), header.begin(), header.end());

        const bool found_route = (raw_route.shortest_path_length != INVALID_EDGE_WEIGHT) &&
                                 (!raw_route.unpacked_path_segments.front().empty());

        if( found_route ) {
            tmp = "<route>\n";

            reply.content.insert(reply.content.end(), tmp.begin(), tmp.end());

            AddRoutePoint(raw_route.segment_end_coordinates.front().source_phantom.location, reply.content);

            for (const std::vector<PathData> &path_data_vector : raw_route.unpacked_path_segments)
            {
                for (const PathData &path_data : path_data_vector)
                {
                    const FixedPointCoordinate current_coordinate =
                        facade->GetCoordinateOfNode(path_data.node);
                    AddRoutePoint(current_coordinate, reply.content);
                }
            }

            AddRoutePoint(raw_route.segment_end_coordinates.back().target_phantom.location, reply.content);

            tmp = "</route>\n<wayids>\n";
            reply.content.insert(reply.content.end(), tmp.begin(), tmp.end());

            long last_id = 0;

            for (const std::vector<PathData> &path_data_vector : raw_route.unpacked_path_segments)
            {
                for (const PathData &path_data : path_data_vector)
                {
                    if (path_data.name_id != last_id) {
                        std::stringstream sstr;
                         sstr << "  <id>" << path_data.name_id << "</id>\n";
                        std::string id = sstr.str();
                        reply.content.insert(reply.content.end(), id.begin(), id.end());
                        last_id = path_data.name_id;
                    }
                }
            }

            tmp = "</wayids>\n";
            reply.content.insert(reply.content.end(), tmp.begin(), tmp.end());
        }

        tmp = "</result>\n";
        reply.content.insert(reply.content.end(), tmp.begin(), tmp.end());
    }
};
#endif // WAYID_DESCRIPTOR_H_
