
// C++ libraries
#include <iostream>

// spdlog
#include <spdlog/sinks/stdout_sinks.h>

// Boost libraries
#include <boost/filesystem.hpp>

// Project headers
#include "lib_clg.hpp"

#include "../streaming_archive/reader/Archive.hpp"
#include "CommandLineArguments.hpp"
#include "../Profiler.hpp"
#include "../compressor_frontend/utils.hpp"
#include "../Grep.hpp"

namespace clg {

static void print_result_text (const string& orig_file_path, const streaming_archive::reader::Message& compressed_msg, const string& decompressed_msg, void* custom_arg) {
    printf("%s:%s", orig_file_path.c_str(), decompressed_msg.c_str());
}

static void print_result_binary (const string& orig_file_path, const streaming_archive::reader::Message& compressed_msg, const string& decompressed_msg, void* custom_arg) {
    bool write_successful = true;
    do {
        size_t length;
        size_t num_elems_written;

        // Write file path
        length = orig_file_path.length();
        num_elems_written = fwrite(&length, sizeof(length), 1, stdout);
        if (num_elems_written < 1) {
            write_successful = false;
            break;
        }
        num_elems_written = fwrite(orig_file_path.c_str(), sizeof(char), length, stdout);
        if (num_elems_written < length) {
            write_successful = false;
            break;
        }

        // Write timestamp
        epochtime_t timestamp = compressed_msg.get_ts_in_milli();
        num_elems_written = fwrite(&timestamp, sizeof(timestamp), 1, stdout);
        if (num_elems_written < 1) {
            write_successful = false;
            break;
        }

        // Write logtype ID
        auto logtype_id = compressed_msg.get_logtype_id();
        num_elems_written = fwrite(&logtype_id, sizeof(logtype_id), 1, stdout);
        if (num_elems_written < 1) {
            write_successful = false;
            break;
        }

        // Write message
        length = decompressed_msg.length();
        num_elems_written = fwrite(&length, sizeof(length), 1, stdout);
        if (num_elems_written < 1) {
            write_successful = false;
            break;
        }
        num_elems_written = fwrite(decompressed_msg.c_str(), sizeof(char), length, stdout);
        if (num_elems_written < length) {
            write_successful = false;
            break;
        }
    } while (false);
    if (!write_successful) {
        SPDLOG_ERROR("Failed to write result in binary form, errno={}", errno);
    }
}

static bool open_compressed_file (streaming_archive::MetadataDB::FileIterator& file_metadata_ix, 
        streaming_archive::reader::Archive& archive, streaming_archive::reader::File& compressed_file) {
    ErrorCode error_code = archive.open_file(compressed_file, file_metadata_ix);
    if (ErrorCode_Success == error_code) {
        return true;
    }
    string orig_path;
    file_metadata_ix.get_path(orig_path);
    if (ErrorCode_FileNotFound == error_code) {
        SPDLOG_WARN("{} not found in archive", orig_path.c_str());
    } else if (ErrorCode_errno == error_code) {
        SPDLOG_ERROR("Failed to open {}, errno={}", orig_path.c_str(), errno);
    } else {
        SPDLOG_ERROR("Failed to open {}, error={}", orig_path.c_str(), error_code);
    }
    return false;
}

static size_t search_files(
        vector<Query>& queries,
        const CommandLineArguments::OutputMethod output_method,
        streaming_archive::reader::Archive& archive,
        streaming_archive::MetadataDB::FileIterator& file_metadata_ix
) {
    size_t num_matches = 0;

    streaming_archive::reader::File compressed_file;
    // Setup output method
    Grep::OutputFunc output_func;
    void* output_func_arg;
    switch (output_method) {
        case CommandLineArguments::OutputMethod::StdoutText:
            output_func = print_result_text;
            output_func_arg = nullptr;
            break;
        case CommandLineArguments::OutputMethod::StdoutBinary:
            output_func = print_result_binary;
            output_func_arg = nullptr;
            break;
        default:
            SPDLOG_ERROR("Unknown output method - {}", (char)output_method);
            return num_matches;
    }

    // Run all queries on each file
    for (; file_metadata_ix.has_next(); file_metadata_ix.next()) {
        if (open_compressed_file(file_metadata_ix, archive, compressed_file)) {
            Grep::calculate_sub_queries_relevant_to_file(compressed_file, queries);

            for (auto const& query : queries) {
                archive.reset_file_indices(compressed_file);
                num_matches += Grep::search_and_output(
                        query,
                        SIZE_MAX,
                        archive,
                        compressed_file,
                        output_func,
                        output_func_arg
                );
            }
        }
        archive.close_file(compressed_file);
    }

    return num_matches;
}

static bool do_search( std::vector<std::string>& search_strings, 
                        CommandLineArguments& command_line_args,
                        streaming_archive::reader::Archive_in_memory& archive,
                        compressor_frontend::lexers::ByteLexer& forward_lexer,
                        compressor_frontend::lexers::ByteLexer& reverse_lexer,
                        bool use_hueristic) {
    // Create vector of search strings
    ErrorCode error_code;
    auto search_begin_ts = command_line_args.get_search_begin_ts();
    auto search_end_ts = command_line_args.get_search_end_ts();

    try {
        vector<Query> queries;
        bool no_queries_match = true;
        std::set<segment_id_t> ids_of_segments_to_search;
        bool is_superseding_query = false;
        for (auto const& search_string : search_strings) {
            Query query;
            if (Grep::process_raw_query(
                        archive,
                        search_string,
                        search_begin_ts,
                        search_end_ts,
                        command_line_args.ignore_case(),
                        query,
                        forward_lexer,
                        reverse_lexer,
                        use_hueristic
                ))
            {
                // if (Grep::process_raw_query(archive, search_string, search_begin_ts,
                // search_end_ts, command_line_args.ignore_case(), query, parser)) {
                no_queries_match = false;

                if (query.contains_sub_queries() == false) {
                    // Search string supersedes all other possible search strings
                    is_superseding_query = true;
                    // Remove existing queries since they are superseded by this one
                    queries.clear();
                    // Add this query
                    queries.push_back(query);
                    // All other search strings will be superseded by this one, so break
                    break;
                }

                queries.push_back(query);

                // Add query's matching segments to segments to search
                for (auto& sub_query : query.get_sub_queries()) {
                    auto& ids_of_matching_segments = sub_query.get_ids_of_matching_segments();
                    ids_of_segments_to_search.insert(
                            ids_of_matching_segments.cbegin(),
                            ids_of_matching_segments.cend()
                    );
                }
            }
        }

        if (!no_queries_match) {
            size_t num_matches;
            if (is_superseding_query) {
                auto file_metadata_ix = archive.get_file_iterator(
                        search_begin_ts,
                        search_end_ts,
                        command_line_args.get_file_path()
                );
                num_matches = search_files(
                        queries,
                        command_line_args.get_output_method(),
                        archive,
                        *file_metadata_ix
                );
            } else {
                auto file_metadata_ix_ptr = archive.get_file_iterator(
                        search_begin_ts,
                        search_end_ts,
                        command_line_args.get_file_path(),
                        cInvalidSegmentId
                );
                auto& file_metadata_ix = *file_metadata_ix_ptr;
                num_matches = search_files(
                        queries,
                        command_line_args.get_output_method(),
                        archive,
                        file_metadata_ix
                );
                for (auto segment_id : ids_of_segments_to_search) {
                    file_metadata_ix.set_segment_id(segment_id);
                    num_matches += search_files(
                            queries,
                            command_line_args.get_output_method(),
                            archive,
                            file_metadata_ix
                    );
                }
            }
            SPDLOG_DEBUG("# matches found: {}", num_matches);
        }
    } catch (TraceableException& e) {
        error_code = e.get_error_code();
        if (ErrorCode_errno == error_code) {
            SPDLOG_ERROR(
                    "Search failed: {}:{} {}, errno={}",
                    e.get_filename(),
                    e.get_line_number(),
                    e.what(),
                    errno
            );
            return false;
        } else {
            SPDLOG_ERROR(
                    "Search failed: {}:{} {}, error_code={}",
                    e.get_filename(),
                    e.get_line_number(),
                    e.what(),
                    error_code
            );
            return false;
        }
    }

    return true;

    }

static bool open_archive(struct clg::Archive_addresses mem_addr, 
    streaming_archive::reader::Archive_in_memory& archive_reader) {
    ErrorCode error_code;

    try {
        // Open archive
        archive_reader.open(mem_addr);
    } catch (TraceableException& e) {
        error_code = e.get_error_code();
        if (ErrorCode_errno == error_code) {
            SPDLOG_ERROR(
                    "Opening archive failed: {}:{} {}, errno={}",
                    e.get_filename(),
                    e.get_line_number(),
                    e.what(),
                    errno
            );
            return false;
        } else {
            SPDLOG_ERROR(
                    "Opening archive failed: {}:{} {}, error_code={}",
                    e.get_filename(),
                    e.get_line_number(),
                    e.what(),
                    error_code
            );
            return false;
        }
    }

    try {
        archive_reader.refresh_dictionaries();
    } catch (TraceableException& e) {
        error_code = e.get_error_code();
        if (ErrorCode_errno == error_code) {
            SPDLOG_ERROR(
                    "Reading dictionaries failed: {}:{} {}, errno={}",
                    e.get_filename(),
                    e.get_line_number(),
                    e.what(),
                    errno
            );
            return false;
        } else {
            SPDLOG_ERROR(
                    "Reading dictionaries failed: {}:{} {}, error_code={}",
                    e.get_filename(),
                    e.get_line_number(),
                    e.what(),
                    error_code
            );
            return false;
        }
    }

    return true;
}



int lib_clg::search(int argc, const char ** argv, struct Archive_addresses& archive_addresses) {
    try {
        auto stderr_logger = spdlog::stderr_logger_st("stderr");
        spdlog::set_default_logger(stderr_logger);
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S,%e [%l] %v");
    } catch (std::exception& e) {
        // NOTE: We can't log an exception if the logger couldn't be constructed
        return -1;
    }
    Profiler::init();
    TimestampPattern::init();

    CommandLineArguments command_line_args("clg");
    // Hack: add a dummy archive directory argument to the command line arguments
    command_line_args.ignore_archive_dir();
    auto parsing_result = command_line_args.parse_arguments(argc, argv);
    switch (parsing_result) {
        case CommandLineArgumentsBase::ParsingResult::Failure:
            return -1;
        case CommandLineArgumentsBase::ParsingResult::InfoCommand:
            return 0;
        case CommandLineArgumentsBase::ParsingResult::Success:
            // Continue processing
            break;
    }

    Profiler::start_continuous_measurement<Profiler::ContinuousMeasurementIndex::Search>();

    // Create vector of search strings
    std::vector<std::string> search_strings;
    if (command_line_args.get_search_strings_file_path().empty()) {
        search_strings.push_back(command_line_args.get_search_string());
    } else {
        FileReader file_reader;
        file_reader.open(command_line_args.get_search_strings_file_path());
        std::string line;
        while (file_reader.read_to_delimiter('\n', false, false, line)) {
            if (!line.empty()) {
                search_strings.push_back(line);
            }
        }
        file_reader.close();
    }

    // Perform search
    /// TODO: if performance is too slow, can make this more efficient by only diffing files with
    /// the same checksum
    const uint32_t max_map_schema_length = 100'000;
    std::map<std::string, compressor_frontend::lexers::ByteLexer> forward_lexer_map;
    std::map<std::string, compressor_frontend::lexers::ByteLexer> reverse_lexer_map;
    compressor_frontend::lexers::ByteLexer one_time_use_forward_lexer;
    compressor_frontend::lexers::ByteLexer one_time_use_reverse_lexer;
    compressor_frontend::lexers::ByteLexer* forward_lexer_ptr{nullptr};
    compressor_frontend::lexers::ByteLexer* reverse_lexer_ptr{nullptr};

    streaming_archive::reader::Archive_in_memory archive_reader;

    // Open archive
    if (!open_archive(archive_addresses, archive_reader)) {
        return -1;
    }

    // perform search
    if(!do_search(  search_strings, 
                    command_line_args,
                    archive_reader,
                    *forward_lexer_ptr,
                    *reverse_lexer_ptr,
                    true))
    {return -1;}

    // Close archive

    Profiler::stop_continuous_measurement<Profiler::ContinuousMeasurementIndex::Search>();
    LOG_CONTINUOUS_MEASUREMENT(Profiler::ContinuousMeasurementIndex::Search)

    return 0;
}
}  // namespace clg
