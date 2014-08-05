#ifndef LENGTH_DESCRIPTOR_H_
#define LENGTH_DESCRIPTOR_H_

#include "BaseDescriptor.h"

#include <boost/foreach.hpp>

template<class DataFacadeT> class LengthDescriptor : public BaseDescriptor<DataFacadeT> {
private:
    DescriptorConfig config;
    FixedPointCoordinate current;
    DataFacadeT * facade;

public:
    LengthDescriptor(DataFacadeT *facade) : facade(facade) {}

    void SetConfig(const DescriptorConfig & c) { config = c; }

    void Run(const RawRouteData &raw_route, http::Reply &reply)
    {
        std::string tmp;

        std::string header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<result>\n";

        reply.content.insert(reply.content.end(), header.begin(), header.end());

        const bool found_route = (raw_route.shortest_path_length != INVALID_EDGE_WEIGHT) &&
                                 (!raw_route.unpacked_path_segments.front().empty());

        if( found_route ) {
            std::stringstream sstr;
            sstr << " <length>" << raw_route.shortest_path_length/1000. << "</length>\n";
            tmp = sstr.str();
            reply.content.insert(reply.content.end(), tmp.begin(), tmp.end());
        }

        tmp = "</result>\n";
        reply.content.insert(reply.content.end(), tmp.begin(), tmp.end());
    }
};
#endif // LENGTH_DESCRIPTOR_H_
