#pragma once
#include "date/tz.h"
#include "../Defs.h"
#include <string>

namespace tz {
    /**
     * @brief Gets the name of current timezone.
     * @return The current timezone IANA name.
    */
    std::string get_local_timezone () ;

    /**
     * @brief Converts a time point from a time point to epoch time.
     * @param time_point The time point to convert.
     * @return The converted epoch time.
    */
    epochtime_t convert_to_epochtime (const std::chrono::system_clock::time_point& time_point) ;

    /**
     * @brief Converts a time point from epoch time to a time point.
     * @param epochtime The epoch time to convert.
     * @return The converted time point.
    */
    std::chrono::system_clock::time_point convert_from_epochtime (epochtime_t& epochtime);

    /**
     * @brief Converts a time point from local to UTC.
     * @param time_point The time point to convert.
     * @return The converted time point.
    */
    std::chrono::system_clock::time_point local_to_utc (const std::chrono::system_clock::time_point& time_point);

    /**
     * @brief Converts a time point from local to UTC.
     * @param time_point The time point to convert.
     * @return The converted time point.
    */
    std::chrono::system_clock::time_point utc_to_local (const std::chrono::system_clock::time_point& time_point) ;

    /**
     * @brief Converts a time point from UTC to a target timezone. 
     *      timezone must be an IANA timezone name, listed: 
     *      https://en.wikipedia.org/wiki/List_of_tz_database_time_zones.
     * @param time_point The time point to convert.
     * @param target_timezone The target timezone.
     * @return The converted time point.
    */
    std::chrono::system_clock::time_point utc_to_target (const std::chrono::system_clock::time_point& time_point, const std::string& target_timezone) ;

    /**
     * @brief Converts a time point from a target timezone to UTC. 
     *      timezone must be an IANA timezone name, listed: 
     *      https://en.wikipedia.org/wiki/List_of_tz_database_time_zones.
     * @param time_point The time point to convert.
     * @param target_timezone The target timezone.
     * @return The converted time point.
    */
    std::chrono::system_clock::time_point target_to_utc (const std::chrono::system_clock::time_point& time_point, const std::string& target_timezone);

    /**
     * @brief Converts a time point from a source timezone to a target timezone. 
     *      timezone must be an IANA timezone name, listed: 
     *      https://en.wikipedia.org/wiki/List_of_tz_database_time_zones.
     * @param time_point The time point to convert.
     * @param source_timezone The source timezone.
     * @param target_timezone The target timezone.
     * @return The converted time point.
    */
    std::chrono::system_clock::time_point source_to_target (const std::chrono::system_clock::time_point& time_point, const std::string& source_timezone, const std::string& target_timezone) ;
}