/*

Copyright (c) 2013, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef JSON_DESCRIPTOR_H_
#define JSON_DESCRIPTOR_H_

#include "BaseDescriptor.h"
#include "DescriptionFactory.h"
#include "../Algorithms/ObjectToBase64.h"
#include "../Algorithms/ExtractRouteNames.h"
#include "../DataStructures/JSONContainer.h"
#include "../DataStructures/Range.h"
#include "../DataStructures/SegmentInformation.h"
#include "../DataStructures/TurnInstructions.h"
#include "../Util/Azimuth.h"
#include "../Util/StringUtil.h"
#include "../Util/TimingUtil.h"

#include <algorithm>

template <class DataFacadeT> class JSONDescriptor : public BaseDescriptor<DataFacadeT>
{
  private:
    DataFacadeT *facade;
    DescriptorConfig config;
    DescriptionFactory description_factory, alternate_description_factory;
    FixedPointCoordinate current;
    unsigned entered_restricted_area_count;
    struct RoundAbout
    {
        RoundAbout() : start_index(INT_MAX), name_id(INVALID_NAMEID), leave_at_exit(INT_MAX) {}
        int start_index;
        unsigned name_id;
        int leave_at_exit;
    } round_about;

    struct Segment
    {
        Segment() : name_id(INVALID_NAMEID), length(-1), position(0) {}
        Segment(unsigned n, int l, unsigned p) : name_id(n), length(l), position(p) {}
        unsigned name_id;
        int length;
        unsigned position;
    };
    std::vector<Segment> shortest_path_segments, alternative_path_segments;
    ExtractRouteNames<DataFacadeT, Segment> GenerateRouteNames;

  public:
    JSONDescriptor(DataFacadeT *facade) : facade(facade), entered_restricted_area_count(0) {}

    void SetConfig(const DescriptorConfig &c) { config = c; }

    unsigned DescribeLeg(const std::vector<PathData> route_leg,
                         const PhantomNodes &leg_phantoms,
                         const bool target_traversed_in_reverse,
                         const bool is_via_leg)
    {
        unsigned added_element_count = 0;
        // Get all the coordinates for the computed route
        FixedPointCoordinate current_coordinate;
        for (const PathData &path_data : route_leg)
        {
            current_coordinate = facade->GetCoordinateOfNode(path_data.node);
            description_factory.AppendSegment(current_coordinate, path_data);
            ++added_element_count;
        }
        description_factory.SetEndSegment(leg_phantoms.target_phantom, target_traversed_in_reverse, is_via_leg);
        ++added_element_count;
        BOOST_ASSERT((route_leg.size() + 1) == added_element_count);
        return added_element_count;
    }

    void Run(const RawRouteData &raw_route, http::Reply &reply)
    {
        JSON::Object json_result;
        if (INVALID_EDGE_WEIGHT == raw_route.shortest_path_length)
        {
            // We do not need to do much, if there is no route ;-)
            json_result.values["status"] = 207;
            json_result.values["status_message"] = "Cannot find route between points";
            JSON::render(reply.content, json_result);
            return;
        }

        // check if first segment is non-zero
        std::string road_name = facade->GetEscapedNameForNameID(
            raw_route.segment_end_coordinates.front().source_phantom.name_id);

        BOOST_ASSERT(raw_route.unpacked_path_segments.size() ==
                     raw_route.segment_end_coordinates.size());

        description_factory.SetStartSegment(
            raw_route.segment_end_coordinates.front().source_phantom,
            raw_route.source_traversed_in_reverse.front());
        json_result.values["status"] = 0;
        json_result.values["status_message"] = "Found route between points";

        // for each unpacked segment add the leg to the description
        for (const auto i : osrm::irange<std::size_t>(0, raw_route.unpacked_path_segments.size()))
        {
#ifndef NDEBUG
            const int added_segments =
#endif
            DescribeLeg(raw_route.unpacked_path_segments[i],
                        raw_route.segment_end_coordinates[i],
                        raw_route.target_traversed_in_reverse[i],
                        raw_route.is_via_leg(i));
            BOOST_ASSERT(0 < added_segments);
        }
        description_factory.Run(facade, config.zoom_level);

        if (config.geometry)
        {
            JSON::Value route_geometry =
                description_factory.AppendEncodedPolylineString(config.encode_geometry);
            json_result.values["route_geometry"] = route_geometry;
        }
        if (config.instructions)
        {
            JSON::Array json_route_instructions;
            BuildTextualDescription(description_factory,
                                    json_route_instructions,
                                    raw_route.shortest_path_length,
                                    shortest_path_segments);
            json_result.values["route_instructions"] = json_route_instructions;
        }
        description_factory.BuildRouteSummary(description_factory.entireLength,
                                              raw_route.shortest_path_length);
        JSON::Object json_route_summary;
        json_route_summary.values["total_distance"] = description_factory.summary.distance;
        json_route_summary.values["total_time"] = description_factory.summary.duration;
        json_route_summary.values["start_point"] =
            facade->GetEscapedNameForNameID(description_factory.summary.source_name_id);
        json_route_summary.values["end_point"] =
            facade->GetEscapedNameForNameID(description_factory.summary.target_name_id);
        json_result.values["route_summary"] = json_route_summary;

        BOOST_ASSERT(!raw_route.segment_end_coordinates.empty());

        JSON::Array json_via_points_array;
        JSON::Array json_first_coordinate;
        json_first_coordinate.values.push_back(
            raw_route.segment_end_coordinates.front().source_phantom.location.lat /
            COORDINATE_PRECISION);
        json_first_coordinate.values.push_back(
            raw_route.segment_end_coordinates.front().source_phantom.location.lon /
            COORDINATE_PRECISION);
        json_via_points_array.values.push_back(json_first_coordinate);
        for (const PhantomNodes &nodes : raw_route.segment_end_coordinates)
        {
            std::string tmp;
            JSON::Array json_coordinate;
            json_coordinate.values.push_back(nodes.target_phantom.location.lat /
                                             COORDINATE_PRECISION);
            json_coordinate.values.push_back(nodes.target_phantom.location.lon /
                                             COORDINATE_PRECISION);
            json_via_points_array.values.push_back(json_coordinate);
        }
        json_result.values["via_points"] = json_via_points_array;

        JSON::Array json_via_indices_array;

        std::vector<unsigned> const &shortest_leg_end_indices = description_factory.GetViaIndices();
        json_via_indices_array.values.insert(json_via_indices_array.values.end(),
                                             shortest_leg_end_indices.begin(),
                                             shortest_leg_end_indices.end());
        json_result.values["via_indices"] = json_via_indices_array;

        // only one alternative route is computed at this time, so this is hardcoded
        if (INVALID_EDGE_WEIGHT != raw_route.alternative_path_length)
        {
            json_result.values["found_alternative"] = JSON::True();
            BOOST_ASSERT(!raw_route.alt_source_traversed_in_reverse.empty());
            alternate_description_factory.SetStartSegment(
                raw_route.segment_end_coordinates.front().source_phantom,
                raw_route.alt_source_traversed_in_reverse.front());
            // Get all the coordinates for the computed route
            for (const PathData &path_data : raw_route.unpacked_alternative)
            {
                current = facade->GetCoordinateOfNode(path_data.node);
                alternate_description_factory.AppendSegment(current, path_data);
            }
            alternate_description_factory.SetEndSegment(raw_route.segment_end_coordinates.back().target_phantom, raw_route.alt_source_traversed_in_reverse.back());
            alternate_description_factory.Run(facade, config.zoom_level);

            if (config.geometry)
            {
                JSON::Value alternate_geometry_string =
                    alternate_description_factory.AppendEncodedPolylineString(
                        config.encode_geometry);
                JSON::Array json_alternate_geometries_array;
                json_alternate_geometries_array.values.push_back(alternate_geometry_string);
                json_result.values["alternative_geometries"] = json_alternate_geometries_array;
            }
            // Generate instructions for each alternative (simulated here)
            JSON::Array json_alt_instructions;
            JSON::Array json_current_alt_instructions;
            if (config.instructions)
            {
                BuildTextualDescription(alternate_description_factory,
                                        json_current_alt_instructions,
                                        raw_route.alternative_path_length,
                                        alternative_path_segments);
                json_alt_instructions.values.push_back(json_current_alt_instructions);
                json_result.values["alternative_instructions"] = json_alt_instructions;
            }
            alternate_description_factory.BuildRouteSummary(
                alternate_description_factory.entireLength, raw_route.alternative_path_length);

            JSON::Object json_alternate_route_summary;
            JSON::Array json_alternate_route_summary_array;
            json_alternate_route_summary.values["total_distance"] =
                alternate_description_factory.summary.distance;
            json_alternate_route_summary.values["total_time"] =
                alternate_description_factory.summary.duration;
            json_alternate_route_summary.values["start_point"] = facade->GetEscapedNameForNameID(
                alternate_description_factory.summary.source_name_id);
            json_alternate_route_summary.values["end_point"] = facade->GetEscapedNameForNameID(
                alternate_description_factory.summary.target_name_id);
            json_alternate_route_summary_array.values.push_back(json_alternate_route_summary);
            json_result.values["alternative_summaries"] = json_alternate_route_summary_array;

            std::vector<unsigned> const &alternate_leg_end_indices =
                alternate_description_factory.GetViaIndices();
            JSON::Array json_altenative_indices_array;
            json_altenative_indices_array.values.insert(json_altenative_indices_array.values.end(),
                                                        alternate_leg_end_indices.begin(),
                                                        alternate_leg_end_indices.end());
            json_result.values["alternative_indices"] = json_altenative_indices_array;
        }
        else
        {
            json_result.values["found_alternative"] = JSON::False();
        }

        // Get Names for both routes
        RouteNames route_names =
            GenerateRouteNames(shortest_path_segments, alternative_path_segments, facade);
        JSON::Array json_route_names;
        json_route_names.values.push_back(route_names.shortest_path_name_1);
        json_route_names.values.push_back(route_names.shortest_path_name_2);
        json_result.values["route_name"] = json_route_names;

        if (INVALID_EDGE_WEIGHT != raw_route.alternative_path_length)
        {
            JSON::Array json_alternate_names_array;
            JSON::Array json_alternate_names;
            json_alternate_names.values.push_back(route_names.alternative_path_name_1);
            json_alternate_names.values.push_back(route_names.alternative_path_name_2);
            json_alternate_names_array.values.push_back(json_alternate_names);
            json_result.values["alternative_names"] = json_alternate_names_array;
        }

        JSON::Object json_hint_object;
        json_hint_object.values["checksum"] = raw_route.check_sum;
        JSON::Array json_location_hint_array;
        std::string hint;
        for (const auto i : osrm::irange<std::size_t>(0, raw_route.segment_end_coordinates.size()))
        {
            EncodeObjectToBase64(raw_route.segment_end_coordinates[i].source_phantom, hint);
            json_location_hint_array.values.push_back(hint);
        }
        EncodeObjectToBase64(raw_route.segment_end_coordinates.back().target_phantom, hint);
        json_location_hint_array.values.push_back(hint);
        json_hint_object.values["locations"] = json_location_hint_array;
        json_result.values["hint_data"] = json_hint_object;

        // render the content to the output array
        TIMER_START(route_render);
        JSON::render(reply.content, json_result);
        TIMER_STOP(route_render);
        SimpleLogger().Write(logDEBUG) << "rendering took: " << TIMER_MSEC(route_render);
    }

    // TODO: reorder parameters
    inline void BuildTextualDescription(DescriptionFactory &description_factory,
                                        JSON::Array &json_instruction_array,
                                        const int route_length,
                                        std::vector<Segment> &route_segments_list)
    {
        // Segment information has following format:
        //["instruction id","streetname",length,position,time,"length","earth_direction",azimuth]
        unsigned necessary_segments_running_index = 0;
        round_about.leave_at_exit = 0;
        round_about.name_id = 0;
        std::string temp_dist, temp_length, temp_duration, temp_bearing, temp_instruction;

        // Fetch data from Factory and generate a string from it.
        for (const SegmentInformation &segment : description_factory.path_description)
        {
            JSON::Array json_instruction_row;
            TurnInstruction current_instruction = segment.turn_instruction;
            entered_restricted_area_count += (current_instruction != segment.turn_instruction);
            if (TurnInstructionsClass::TurnIsNecessary(current_instruction))
            {
                if (TurnInstruction::EnterRoundAbout == current_instruction)
                {
                    round_about.name_id = segment.name_id;
                    round_about.start_index = necessary_segments_running_index;
                }
                else
                {
                    std::string current_turn_instruction;
                    if (TurnInstruction::LeaveRoundAbout == current_instruction)
                    {
                        temp_instruction =
                            IntToString(as_integer(TurnInstruction::EnterRoundAbout));
                        current_turn_instruction += temp_instruction;
                        current_turn_instruction += "-";
                        temp_instruction = IntToString(round_about.leave_at_exit + 1);
                        current_turn_instruction += temp_instruction;
                        round_about.leave_at_exit = 0;
                    }
                    else
                    {
                        temp_instruction = IntToString(as_integer(current_instruction));
                        current_turn_instruction += temp_instruction;
                    }
                    json_instruction_row.values.push_back(current_turn_instruction);

                    json_instruction_row.values.push_back(
                        facade->GetEscapedNameForNameID(segment.name_id));
                    json_instruction_row.values.push_back(std::round(segment.length));
                    json_instruction_row.values.push_back(necessary_segments_running_index);
                    json_instruction_row.values.push_back(round(segment.duration / 10));
                    json_instruction_row.values.push_back(
                        UintToString(static_cast<int>(segment.length)) + "m");
                    const double bearing_value = (segment.bearing / 10.) ;
                    json_instruction_row.values.push_back(Azimuth::Get(bearing_value));
                    json_instruction_row.values.push_back(static_cast<unsigned>(round(bearing_value)));

                    route_segments_list.emplace_back(
                        segment.name_id, static_cast<int>(segment.length), static_cast<unsigned>(route_segments_list.size()));
                    json_instruction_array.values.push_back(json_instruction_row);
                }
            }
            else if (TurnInstruction::StayOnRoundAbout == current_instruction)
            {
                ++round_about.leave_at_exit;
            }
            if (segment.necessary)
            {
                ++necessary_segments_running_index;
            }
        }

        JSON::Array json_last_instruction_row;
        temp_instruction = IntToString(as_integer(TurnInstruction::ReachedYourDestination));
        json_last_instruction_row.values.push_back(temp_instruction);
        json_last_instruction_row.values.push_back("");
        json_last_instruction_row.values.push_back(0);
        json_last_instruction_row.values.push_back(necessary_segments_running_index - 1);
        json_last_instruction_row.values.push_back(0);
        json_last_instruction_row.values.push_back("0m");
        json_last_instruction_row.values.push_back(Azimuth::Get(0.0));
        json_last_instruction_row.values.push_back(0.);
        json_instruction_array.values.push_back(json_last_instruction_row);
    }
};

#endif /* JSON_DESCRIPTOR_H_ */
