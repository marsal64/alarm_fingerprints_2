//========================================
// Program	   : alarm_fingerprints
// Author      : Martin Saly
// Version     : 2.xx
// Description : Fingerprints based alarm 
//========================================
#define AF_PROGRAM_VERSION "2.5"

/*
 standalone program version to be run using:
 alarm_fingerprints [options] < datafile

 e.g.:
 ./alarm_fingerprints -s 1 -p 5 -d 2 < testdata.csv
 ./alarm_fingerprints -s5 < testdata.csv

 testdata.csv should have expected structure timestamp; value, e.g.:

 time;  avg; event
 [dd-mm-yyyy hh:mm:ss.usec]; [au]; [--]
 10-03-2016 15:19:20.729915 ;   68998
 10-03-2016 15:19:20.729979 ;   69058
 10-03-2016 15:19:20.730043 ;   68808

 (first two lines will be omitted here, see -x parameter below)


 and options correspond to the default parameter / initial value described below:

 -a corresponds to: number_of_points_to_alarm
 -b corresponds to: use_diff_value
 -c corresponds to: distance_calculation_type
 -d corresponds to: debug_level
 -e corresponds to: fingerprint_match_positives_from
 -f corresponds to: fingerprint_match_positives_to
 -g corresponds to: generate_fingerprints
 -h corresponds to: print_help
 -i corresponds to: initial_avg_diff
 -j corresponds to: fingerprint_match_negatives_from
 -k corresponds to: fingerprint_match_negatives_to
 -l corresponds to: fingerprint_length
 -m corresponds to: multiplicator_to_detect
 -n corresponds to: n_amend_avgdiff
 -o corresponds to: matchdistance_to_output
 -p corresponds to: fingerprints_directory
 -r corresponds to: matches_evaluation_logic
 -s corresponds to: sample_each
 -t corresponds to: genpattern_hour_limit
 -u corresponds to: wait_state_usec
 -w corresponds to: wavelet_function
 -x corresponds to: skip_if_contains
 -y corresponds to: matching_distance_positives_max
 -z corresponds to: matching_distance_negatives_max

 After processing, following data are outputted (if -d is 1):
 (I) Parameters values overview

 (II) Line by line info
 lineid - current line (measurement point)
 timestamp - timestamp value
 meas - measured value
 diff - difference of the measured value from the previous value
 diffavg - current average absolute value difference  value used for thresholding
 isdetect - detection of alarm in progress (counting, see number_of_points_to_alarm)
 isalarm - if alarm_noisereject algorithm alarm is raised, 1 is printed, else 0
 iswait - if waiting status, 1 is printed (see wait_state_usec), else 0
 patternid - if no pattern recognition status, prints 0 else prints sequential number of recognized pattern at each related line
 isfinalmatch - 0 if no final match based on current logic, 1 if final match raised
 matchdistance - matching distance (see documentation)
 contivalue - "continuous value" (see documentation)
 outputvalue - either matchdistance or contivalue, based on matchdistance_to_output (-o) parameter

 If not forbidden by -g, for each found pattern, one file is generated to specified directory with name:
 w_N_TIMESTAMP.fpr_WAV_LENGTH

 where:
 N - sequential id of pattern found in the input data
 TIMESTAMP - timetamp where pattern starts
 WAV - number for selected wavelet (e.g. 12 for daub12)
 LENTGH - length of the fingerprint pattern, e.g. 1024

 */

#include <iostream>
#include <string>
#include <iomanip>
#include <sys/time.h>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <unistd.h>
#include <dirent.h>
#include <regex>

#include "alarm_fingerprints_2.hpp"



int main(int argc, char* argv[]) {

	// initialize parameter variables (for comments see .hpp)
	_print_help = 0;
	_sample_each = 1;
	_initial_avg_diff = 10000;
	_n_amend_avgdiff = 500;
	_number_of_points_to_alarm = 5;
	_multiplicator_to_detect = 10;
	_wait_state_usec = 1000000;
	_distance_calculation_type = 1;
	_fingerprint_length = 1024;
	_fingerprint_match_positives_from = 0;
	_fingerprint_match_negatives_from = 0;
	_fingerprint_match_positives_to = 511;
	_fingerprint_match_negatives_to = 511;
	_wavelet_function = 2;
	_wav_func = &daub2_transform; // default pointer to function
	_generate_fingerprints = 0;
	_matching_distance_positives_max = 0.5;
	_matching_distance_negatives_max = 0.5;
	_matches_evaluation_logic = 1;
	_skip_if_contains = "m";
	_use_diff_value = 0;
	_fingerprints_directory = "./";
	_debug_level = 0;
	_matchdistance_to_output = 0;
	_genpattern_hour_limit = 0;


	// debug
	// std::cout << "(debug) argc:" << argc << std::endl;
	// std::cout << "(debug) argv[1]:" << argv[1] << std::endl;


	// parse and amend arguments (if present) ///////////////////////////////////////////////
	opterr = 0;
	while ((co = getopt(argc, argv,
			"a:bc:d:e:f:g:hi:j:k:l:m:n:op:r:s:t:u:w:x:y:z:")) != EOF) {

		//debug
		// std::cout << "(debug) co: " << (char) co << std::endl;
		// std::cout << "(debug) optarg: " << optarg << std::endl;

		switch (co) {

		case 'h':
			_print_help = 1;
			break;

		case 'o':
			_matchdistance_to_output = 1;
			break;

		case 'p':
			_fingerprints_directory = optarg;
			break;

		case 'b':
			_use_diff_value = 1;
			break;

		case 'c':
			try {
				_distance_calculation_type = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}

			if (!(_distance_calculation_type == 1
					|| _distance_calculation_type == 2)) {
				DLOG(LOG_ERROR,
						"distance_calculation_type (-c) must be 1 or 2\nExiting");
				return 1;
			}
			break;

		case 'g':
			try {
				_generate_fingerprints = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_generate_fingerprints >= 0 && _generate_fingerprints <= 2)) {
				DLOG(LOG_ERROR,
						"generate_fingerprints (-g) must be 0, 1 or 2\nExiting");
				return 1;
			}
			break;

		case 'r':
			try {
				_matches_evaluation_logic = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_matches_evaluation_logic >= 0 && _matches_evaluation_logic <= 4)) {
				DLOG(LOG_ERROR,
						"matches_evaluation_logic (-r) must be in interval 0..4\nExiting");
				return 1;
			}
			break;
		case 's':
			try {
				_sample_each = std::stoi(optarg);
			}

			catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_sample_each >= 1)) {
				DLOG(LOG_ERROR, "sample_each (-s) must be >= 1\nExiting");
				return 1;
			}
			break;

		case 'i':
			try {
				_initial_avg_diff = std::stod(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_initial_avg_diff > 0)) {
				DLOG(LOG_ERROR, "initial_avg_diff (-i) must be > 0\nExiting");
				return 1;
			}
			break;
		case 'n':
			try {
				_n_amend_avgdiff = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_n_amend_avgdiff >= 1)) {
				DLOG(LOG_ERROR, "n_amend_avgdiff (-i) must be >= 1\nExiting");
				return 1;
			}
			break;
		case 'a':
			try {
				_number_of_points_to_alarm = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_number_of_points_to_alarm >= 1)) {
				DLOG(LOG_ERROR,
						"_number_of_points_to_alarm (-a) must be >= 1\nExiting");
				return 1;
			}
			break;

		case 'm':
			try {
				_multiplicator_to_detect = std::stod(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_multiplicator_to_detect > 0)) {
				DLOG(LOG_ERROR,
						"multiplicator_to_detect (-m) must be >0\nExiting");
				return 1;
			}
			break;

		case 'u':
			try {
				_wait_state_usec = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_wait_state_usec >= 0)) {
				DLOG(LOG_ERROR, "wait_state_usec (-u) must be >=0\nExiting");
				return 1;
			}
			break;

		case 'l':
			try {
				_fingerprint_length = std::stoi(optarg);

			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_fingerprint_length >= 8)) {
				DLOG(LOG_ERROR,
						"\nfingerprint_length (-l) must be >=8\nExiting");
				return 1;
			}
			break;

		case 'e':
			try {
				_fingerprint_match_positives_from = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_fingerprint_match_positives_from >= 0)) {
				DLOG(LOG_ERROR,
						"\nfingerprint_match_positives_from (-e) must be >=0\nExiting");
				return 1;
			}
			break;
		case 'f':
			try {
				_fingerprint_match_positives_to = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_fingerprint_match_positives_to > 0)) {
				DLOG(LOG_ERROR,
						"\nfingerprint_match_positives_to (-f) must be > 0\nExiting");
				return 1;
			}
			break;

		case 'j':
			try {
				_fingerprint_match_negatives_from = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_fingerprint_match_negatives_from >= 0)) {
				DLOG(LOG_ERROR,
						"\nfingerprint_match_negatives_from (-j) must be >=0\nExiting");
				return 1;
			}
			break;
		case 'k':
			try {
				_fingerprint_match_negatives_to = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_fingerprint_match_negatives_to > 0)) {
				DLOG(LOG_ERROR,
						"\nfingerprint_match_negatives_to (-k) must be > 0\nExiting");
				return 1;
			}
			break;
		case 't':
			try {
				_genpattern_hour_limit = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_genpattern_hour_limit >= 0)) {
				DLOG(LOG_ERROR,
						"\ngenpattern_hour_limit (-t) must be 0 or >=1\nExiting");
				return 1;
			}
			break;

		case 'w':
			try {
				_wavelet_function = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}

			switch (_wavelet_function) {
			case 2:
				_wav_func = &daub2_transform;
				break;
			case 4:
				_wav_func = &daub4_transform;
				break;
			case 6:
				_wav_func = &daub6_transform;
				break;
			case 8:
				_wav_func = &daub8_transform;
				break;
			case 10:
				_wav_func = &daub10_transform;
				break;
			case 12:
				_wav_func = &daub12_transform;
				break;
			case 14:
				_wav_func = &daub14_transform;
				break;
			case 16:
				_wav_func = &daub16_transform;
				break;
			case 18:
				_wav_func = &daub18_transform;
				break;
			case 20:
				_wav_func = &daub20_transform;
				break;
			default:
				DLOG(LOG_ERROR,
						"With -w use one of the following:\n2,4,6,8,10,12,14,16,18,20\nExiting");
				return 1;
			}
			break;

		case 'y':
			try {
				_matching_distance_positives_max = std::stod(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_matching_distance_positives_max >= 0
					&& _matching_distance_positives_max <= 1)) {
				DLOG(LOG_ERROR,
						"\nmatching_distance_positives_max (-y) must be >=0 and <= 1\nExiting");
				return 1;
			}
			break;

		case 'z':
			try {
				_matching_distance_negatives_max = std::stod(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_matching_distance_negatives_max >= 0
					&& _matching_distance_negatives_max <= 1)) {
				DLOG(LOG_ERROR,
						"\nmatching_distance_negatives_max (-z) must be >=0 and <= 1\nExiting");
				return 1;
			}
			break;

		case 'x':
			_skip_if_contains = optarg;
			break;

		case 'd':
			try {
				_debug_level = std::stoi(optarg);
			} catch (const std::exception &exc) {
				DLOG(LOG_ERROR,
						"\nParameter error near " + std::string(1, co)
								+ "\nExiting");
				return 1;
			}
			if (!(_debug_level >= 0 && _debug_level <= 2)) {
				DLOG(LOG_ERROR,
						"\ndebug_level (-d) must be 0, 1, or 2\nExiting");
				return 1;
			}
			break;
		case '?':
			// debug
			//std::cout << "(debug) arg: " << (char) optopt << std::endl;
			DLOG(LOG_ERROR,
					"\nParameter error or wrong parameter value\nExiting");
			return 1;

		default:
			DLOG(LOG_ERROR, "\nArguments parsing error\nExiting");
			return 1;
		}

	}

	// print help
	if (_print_help) {
		LOG(LOG_INFO,
				"\n*Standalone version usage: alarm_fingerprints_2 [parameters] < inputfile");
	}

	// print arguments
	if (_debug_level || _print_help) {
		_lmessage =
				"\n*Starting alarm_fingerprints, version "+std::string(AF_PROGRAM_VERSION)+"\n*"
						+ std::string(116, '=')
						+ "\n*Run parameters:\n"
						+ "*(-s) sample_each="
						+ std::to_string(_sample_each)
						+ "   (integer, should be >= 1)\n"
						+ "*(-i) initial_avg_diff="
						+ std::to_string(_initial_avg_diff)
						+ "   (float, should be > 0)\n"
						+ "*(-n) n_amend_avgdiff="
						+ std::to_string(_n_amend_avgdiff)
						+ "   (integer, should be >= 1)\n"
						+ "*(-a) number_of_points_to_alarm="
						+ std::to_string(_number_of_points_to_alarm)
						+ "   (integer, should be >= 1)\n"
						+ "*(-m) multiplicator_to_detect="
						+ std::to_string(_multiplicator_to_detect)
						+ "   (float, should be >= 1)\n"
						+ "*(-u) wait_state_usec="
						+ std::to_string(_wait_state_usec)
						+ "   (integer, should be >= 0)\n"
						+ "*(-l) fingerprint_length="
						+ std::to_string(_fingerprint_length)
						+ "   (integer, should be >= 8 and power of 2)\n"
						+ "*(-e) fingerprint_match_positives_from="
						+ std::to_string(_fingerprint_match_positives_from)
						+ "   (integer, should be >=0)\n"
						+ "*(-f) fingerprint_match_positives_to="
						+ std::to_string(_fingerprint_match_positives_to)
						+ "   (integer, should be >= 1 and <= fingerprint_length)\n"
						+ "*(-t) genpattern_hour_limit="
						+ std::to_string(_genpattern_hour_limit)
						+ "   (integer, should be 0 (unlimited) or >= 1)\n"
						+ "*(-h) print_help=" + std::to_string(_print_help)
						+ "   (integer, should be 0 (not present) or 1 (present) )\n"
						+ "*(-j) fingerprint_match_negatives_from="
						+ std::to_string(_fingerprint_match_negatives_from)
						+ "   (integer, should be >= 0)\n"
						+ "*(-k) fingerprint_match_negatives_to="
						+ std::to_string(_fingerprint_match_negatives_to)
						+ "   (integer, should be >= 1 and <= fingerprint_length)\n"
						+ "*(-w) wavelet_function="
						+ std::to_string(_wavelet_function)
						+ "   (integer, with one of following values: 2,4,6,8,10,12,14,16,18,20)\n"
						+ "*(-c) distance_calculation_type="
						+ std::to_string(_distance_calculation_type)
						+ "   (integer, should be 1 or 2)\n"
						+ "*(-g) generate_fingerprints="
						+ std::to_string(_generate_fingerprints)
						+ "   (integer, value 0,1 or 2)\n"
						+ "*(-y) matching_distance_positives_max="
						+ std::to_string(_matching_distance_positives_max)
						+ "   (float, should be within interval 0..1)\n"
						+ "*(-z) matching_distance_negatives_max="
						+ std::to_string(_matching_distance_negatives_max)
						+ "   (float, should be within interval 0..1)\n"
						+ "*(-r) matches_evaluation_logic="
						+ std::to_string(_matches_evaluation_logic)
						+ "   (integer, should be within interval 0..4)\n"
						+ "*(-x) skip_if_contains='" + _skip_if_contains
						+ "'   (string)\n" + "*(-b) use_diff_value="
						+ std::to_string(_use_diff_value)
						+ "   (integer, should be 0 (not present) or 1 (present) )\n"
						+ "*(-o) matchdistance_to_output="
						+ std::to_string(_matchdistance_to_output)
						+ "   (string)\n" + "*(-p) fingerprints_directory='"
						+ _fingerprints_directory
						+ "'   (string, './' means 'current directory')\n"
						+ "*(-d) debug_level=" + std::to_string(_debug_level)
						+ "   (integer, should be 0, 1 or 2)\n" + "*"
						+ std::string(116, '=');

		LOG(LOG_INFO, _lmessage);
		;
	}

	// exit if _print_help
	if (_print_help) {
		LOG(LOG_INFO, "\n*Parameter -h found, exiting");

#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
		return 1;
#else
		{
			//helper code for ProcessGuard\n//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);
		}
#endif
	}

	// lightweight arguments logical validity evaluation
	if (_sample_each <= 0 || _initial_avg_diff <= 0 || _n_amend_avgdiff <= 0
			|| _number_of_points_to_alarm <= 0 || _multiplicator_to_detect <= 0
			|| _wait_state_usec < 0 || _fingerprint_length <= 0
			|| _fingerprint_match_positives_from < 0
			|| _fingerprint_match_negatives_from < 0
			|| _matching_distance_positives_max < 0
			|| _matching_distance_negatives_max < 0) {
		DLOG(LOG_ERROR, "\n*Arguments values check not passed\nExiting");

#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
		return 1;
#else
		{
			//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);
		}

#endif

	}

	// amend _fingerprint_length to nearest lowe power of 2 (duplicated for standalone version)
	// debug
	// std::cout << "(debug) _fingerprint_length: "<< _fingerprint_length << std::endl;
	//std::cout << "(debug) log2(_fingerprint_length): " << log2(_fingerprint_length) << std::endl;
	//std::cout << "(debug) trunc (log2(_fingerprint_length)): " << trunc(log2(_fingerprint_length)) << std::endl;

	if (_fingerprint_length != pow(2, trunc(log2(_fingerprint_length)))) {
		_fingerprint_length = pow(2, trunc(log2(_fingerprint_length)));

		if (_debug_level)
			LOG(LOG_WARNING,
					"*WARNING: To match power of 2, fingerprint_length amended to "
							+ std::to_string(_fingerprint_length));
	}

	// amend _fingerprint_match_positives_to and _fingerprint_match_negatives_to
	if (_fingerprint_match_positives_to > _fingerprint_length - 1) {
		_fingerprint_match_positives_to = _fingerprint_length - 1;
		if (_debug_level)
			LOG(LOG_WARNING,
					"*WARNING: Based on fingerprint_length, fingerprint_match_positives_to amended to "
							+ std::to_string(_fingerprint_match_positives_to));
	}

	if (_fingerprint_match_negatives_to > _fingerprint_length - 1) {
		_fingerprint_match_negatives_to = _fingerprint_length - 1;
		if (_debug_level)
			LOG(LOG_WARNING,
					"*WARNING: Based on fingerprint_length, fingerprint_match_negatives_to amended to "
							+ std::to_string(_fingerprint_match_negatives_to));
	}

	// processing start //////////////////////////////////////////////////////////////

	// set precision to double (mainly for fingerprints file generating)
	std::cout.precision(17);


	/////////////////////////////////////////////////////////////////////////////////
	// working variables

	// resize vector for fingerprints
	_fngpts.resize(MAX_FINGEPRINTS_TO_LOAD);

	// for comments, see .hpp
	_diffavg = _initial_avg_diff;
	_fingerprint_n = _fingerprint_length;
	_patzeros = 0;
	_cursample = _sample_each;

	_isalarm = 0;
	_iswait = 0;
	_numthresholded = _number_of_points_to_alarm;
	_lasttime = { 2147483646, 0 }; // large enough
	_curtime = _lasttime;
	_alarmraisetime = { 0, 0 };    // small enough

	_genpattern_time = { 0, 0 };
	_genpattern_count = 0;

	// variable to count number of input lines
	_lineid = 0;

	_diff = 0; // difference from previous value, absolute value
	_diffnoabs = 0; // noabsvalue

	// patterns related variables
	_patternid = 0;
	_ispattern = 0; 	// status variable for pattern tracking
	_patterncount = 0; 	// counts current pattern length
	_ismatch = 0;		// variable indicating final match
	_numberfps = 0; // "real" counter of fingerprints

	_matchdistance_out = -1;
	_contivalue = 0;

	//////////////////////////////////////////
	// load negative and positive fingerprints
	//
	// filename p_xxxxxxx.fprxxxx holds positives, x is any or none character
	// filename n_xxxxxxx.fprxxxx holds negatives, x is any or none character
	//////////////////////////////////////////

	// construct list of potential filenames to _patfilenames vector
	_matchstr1 = "p_.*\\.fpr.*";
	_matchstr2 = "n_.*\\.fpr.*";

	if ((_dir = opendir(&_fingerprints_directory[0])) != NULL) {
		if (_debug_level) {
			_lmessage =
					"\n*Searching for fingerprint patterns, patterns bank expected in directory '"
							+ _fingerprints_directory + "':";
			LOG(LOG_INFO, _lmessage);
		}
		while ((_ent = readdir(_dir)) != NULL) {
			_hstr = _ent->d_name;
			if (std::regex_match(_hstr, _matchstr1)
					|| std::regex_match(_hstr, _matchstr2)) {
				if (_debug_level) {
					// debug
					//std::cocurrentut << "(debug) type of: " << typeid(ent->d_name).name() << std::endl;
					_lmessage = "*Filename '" + _hstr
							+ "' found in patterns bank";
					LOG(LOG_INFO, _lmessage);
				}

				// store filename
				_patfilenames.push_back(_hstr);
			}
		}
		closedir(_dir);
	} else {
		DLOG(LOG_ERROR,
				"Cannot open directory '" + _fingerprints_directory
						+ "' for patterns (may verify -p parameter)\nExiting");
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
		return 1;
#else
		{
			//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);
		}
#endif
	}

	// finish if too much _patfilenames
	if (_patfilenames.size() > MAX_FINGEPRINTS_TO_LOAD) {
		DLOG(LOG_ERROR, "Too many files with pattern-like name found\nExiting");
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
		return 1;
#else
		{
			//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);
		}
#endif

	}

	// load fingerprints files
	if (_patfilenames.size() > 0) {

		std::string linefile; // helper for reads
		std::string fn; // helper for file name
		std::string patname; // helper for patterns name

		if (_debug_level)
			LOG(LOG_INFO, "\n*Loading fingerprint patterns from bank");

		// debug
		//std::cout << "(debug) _patfilenames.size: " << _patfilenames.size() << std::endl;

		for (unsigned long i = 0; i < _patfilenames.size(); i++) {

			fn = _patfilenames[i];

			// debug
			// std::cout << "(debug) patfilename[i]: " << fn << std::endl;

			std::ifstream lfpfile(_fingerprints_directory+fn);
			if (lfpfile.is_open()) {

				// file is open, now parse patnumber and exit if not valid (exactly e.g. p_0001_)
				try {
					patname = fn.substr(0, fn.find('.', 1));

				} catch (const std::exception &exc) {
					DLOG(LOG_ERROR,
							"Error parsing fingerprint name from file name: "
									+ fn + "\nExiting\n");
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
					return 1;
#else
					{
						//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);

					}
#endif
				}

				while (std::getline(lfpfile, linefile)) {
					try {

						// debug
						// std::cout << "(debug) push_back: " << std::stod(linefile) << std::endl;
						// std::cout << "(debug) push_back to i: " << i << std::endl;
						// std::cout << "(debug) current size _fngpts[i]: " << _fngpts[i].size() << std::endl;

						_fngpts[i].push_back(std::stod(linefile));

					} catch (const std::exception &exc) {
						DLOG(LOG_ERROR,
								"Error reading fingerprint file "
										+ _patfilenames[i] + "\nLine: "
										+ linefile + "\nExiting\n");
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
						return 1;
#else
						{
							//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);

						}
#endif
					}
				}

				lfpfile.close();

				if (_debug_level > 1) {
					LOG(LOG_INFO,
							"\n*Loaded fingerprint pattern '" + patname
									+ "' with values:");
					_lmessage = "*";
					for (unsigned int j = 0; j < _fngpts[i].size(); j++)
						_lmessage = _lmessage + std::to_string(_fngpts[i][j])
								+ " ";
					LOG(LOG_INFO, _lmessage);
				}

				// fingerprint seems to be read: push fingerprint name

				// debug
				//std::cout << "(debug) push patname: " << patname << std::endl;

				_fngptsnames.push_back(patname);
				_numberfps++;

				// debug
				//std::cout << "(debug) fngptnames size: " << _fngptsnames.size() << std::endl;

			}

			else {
				_lmessage = "\nUnable to open file " + _patfilenames[i]
						+ "\nExiting";
				DLOG(LOG_ERROR, _lmessage);
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
				return 1;
#else
				{
					//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);

				}
#endif
			}
		}

		// overview of loaded patterns
		if (_debug_level) {
			LOG(LOG_INFO, "\n*Summary of fingerprint patterns load from bank:");
			LOG(LOG_INFO,
					"\n*Number of patterns loaded: "
							+ std::to_string(_numberfps));
			for (int i = 0; i < _numberfps; i++) {
				_lmessage = "*Pattern '" + _fngptsnames[i] + "' has length "
						+ std::to_string(_fngpts[i].size());

				// check fingeprints lengths consistency
				if (_fngpts[i].size() > (unsigned int) _fingerprint_length)
					_lmessage = _lmessage
							+ ", WARNING: this exceeds fingeprint_length="
							+ std::to_string(_fingerprint_length) + "!";
				LOG(LOG_WARNING, _lmessage);
			}

		}

	} else {
		// no patterns found
		if (_debug_level)
			LOG(LOG_INFO, "*No patterns found in bank for load");
	}

	// output header in debug > 1

	if (_debug_level)
		LOG(LOG_INFO, "\n*Measurements processing start");

	if (_debug_level > 1) {
		LOG(LOG_INFO,
				"*" + std::string(117, '=')
						+ "\nlineid;timestamp;meas;diff;diffavg;isdetect;isalarm;iswait;patternid;isfinalmatch;matchdistance;contivalue;outputvalue");
	}

	while (std::getline(std::cin, _lineread)) {

		// debug output: copy of input line
		// std::cout << std::endl << _lineread << std::endl;

		// sampling?
		if (_cursample-- > 1) {

			// debug
			// std::cout << "previous line sampled out" << std::endl;
			continue;
		} else
			_cursample = _sample_each;

		// patch: tries to skip "standard" heading lines containing certain character
		if (_lineread.find(_skip_if_contains) != std::string::npos) {
			// debug
			// std::cout << "(debug) skipped line" << std::endl;
			continue;
		}

		// parses input line
		pos = _lineread.find(';');

		// skips if not found ';'
		if (pos == std::string::npos)
			continue;

		_p1 = trim(_lineread.substr(0, pos)); // parse and trim first before ;, i.e. timestamp
		_p2 = trim(_lineread.erase(0, pos + 1)); // parse and trim measured value

		// convert string to char to be used by sscanf
		c = &_p1[0];

		// parse using sscanf
		// e.g. 10-03-2016 15:19:20.729915
		sscanf(c, "%d-%d-%d %d:%d:%d.%d", &d, &m, &y, &h, &mi, &s, &ms);

		// fill current time
		t.tm_year = y - 1900;
		t.tm_mon = m;
		t.tm_mday = d;
		t.tm_hour = h;
		t.tm_min = mi;
		t.tm_sec = s;
		_curtime.tv_sec = mktime(&t);
		_curtime.tv_usec = ms;

		_lasttime = _curtime;

		// take current value to _curval
		_curval = std::stod(_p2);

		// increments lineid and copies last values for the first line
		if (!_lineid++)
			_lastval = _curval;

		// calculate diff as abs value
		_diffnoabs = _curval - _lastval;
		_diff = fabs(_diffnoabs);

		// pattern evaluation
		if (_ispattern == 1) {

			// is in "pattern" state - push noabs diff or meas based on use_diff_value
			if (_use_diff_value)
				_seqdata.push_back(_diffnoabs);
			else
				_seqdata.push_back(_curval);

			// debug
			// std::cout << "pattern recognition" << std::endl;
			// std::cout << "_patterncount: " << _patterncount << std::endl;

			if (--_patterncount <= 0) {
				// pattern end, process _seqdata
				_dw = &_seqdata[0];

				//debug
				//std::cout << "Input data for wavelet:\n";
				//for (int i = 0; i < _fingerprint_n; i++ )
				//	std::cout << _dw[i] << std::endl;

				// find fingerprint (uses pointer to wavelet function)
				_vw = (*_wav_func)(_fingerprint_n, _dw);

				// write fingerprint to log
				if (_debug_level > 1) {
					LOG(LOG_INFO,
							"*Measurements for actual pattern collected, calculated fingerprint:");
					_lmessage = "*";
					for (unsigned int j = 0; j < _seqdata.size() - 1; j++)
						_lmessage = _lmessage + std::to_string(_vw[j]) + " ";
					LOG(LOG_INFO, _lmessage);
				}

				// if patterns for comparison exist, do matching between
				// current pattern i _vw and all patterns where:
				// _numberfps - number of loaded patterns
				// _fngpts - vector of vectors of loaded patterns
				// _fngptsnames - vector of loaded patterns names
				//              (_n.... for negatives or _p.... for positives)
				// matches_evaluation_logic drives how patterns are matched

				if (_debug_level) {

					// only if some patterns loaded
					if (_numberfps) {
						_lmessage =
								"*Matching collected measurements pattern with patterns loaded"
										" from bank (matching logic="
										+ std::to_string(
												_matches_evaluation_logic)
										+ "):";
						LOG(LOG_INFO, _lmessage);
					}

				}

				_pos_count = 0;
				_matchpos_count = 0;
				_neg_count = 0;
				_matchneg_count = 0;
				_matchdistance_pos_min = 1; // initial max value
				_matchdistance_neg_min = 1; // initial max value

				// evaluate positives if necessary
				if (_matches_evaluation_logic == 2
						|| _matches_evaluation_logic == 3
						|| _matches_evaluation_logic == 4) {

					// try to match EACH negative match
					for (int i = 0; i < _numberfps; i++) {
						if (_fngptsnames[i].substr(0, 1) == "p") {

							_pos_count++;

							// calculate distance using parameters
							_matchdistance = _eucl_dist(_vw, &_fngpts[i][0],
									_fingerprint_match_positives_from,
									_fingerprint_match_positives_to,
									_fingerprint_length,
									_distance_calculation_type);

							// amend min found positive distances
							if (_matchdistance < _matchdistance_pos_min)
								_matchdistance_pos_min = _matchdistance;

							if (_debug_level) {
								_lmessage =
										"*Actual pattern no "
												+ std::to_string(_patternid)
												+ " and positive bank pattern '"
												+ _fngptsnames[i]
												+ "', matching items "
												+ std::to_string(
														_fingerprint_match_positives_from)
												+ ".."
												+ std::to_string(
														_fingerprint_match_positives_to)
												+ ", threshold="
												+ std::to_string(
														_matching_distance_positives_max)
												+ ", match distance is "
												+ std::to_string(_matchdistance)
												+ "  "
												+ ((_matchdistance
														<= _matching_distance_positives_max) ?
														"* individual match *" :
														"* no individual match *");
								LOG(LOG_INFO, _lmessage);
							}

							// match (first positive match is enough)
							if (_matchdistance
									<= _matching_distance_positives_max) {

								_matchpos_count++;
								_matchtestposname = _fngptsnames[i];

							}
						}

						// break for loop if any positive when evaluation logic 2
						// continuing in evaluation when evaluation logic 3 or 4
						if (_matchpos_count
								&& (_matches_evaluation_logic == 2
										|| _matches_evaluation_logic == 3))
							break;
					}

				}

				// evaluate negatives (if necessary)
				if (_matches_evaluation_logic == 1
						|| _matches_evaluation_logic == 3) {

					// try to match EACH negative match
					for (int i = 0; i < _numberfps; i++) {
						if (_fngptsnames[i].substr(0, 1) == "n") {

							_neg_count++;

							// calculate distance using parameters
							_matchdistance = _eucl_dist(_vw, &_fngpts[i][0],
									_fingerprint_match_negatives_from,
									_fingerprint_match_negatives_to,
									_fingerprint_length,
									_distance_calculation_type);

							if (_matchdistance < _matchdistance_neg_min)
								_matchdistance_neg_min = _matchdistance;

							// debug
							//std::cout << "(debug) matchdistance: " << _matchdistance << std::endl;

							if (_matchdistance
									<= _matching_distance_negatives_max) {

								_matchneg_count++;
							}

							if (_debug_level) {

								_lmessage =
										"*Actual pattern no "
												+ std::to_string(_patternid)
												+ " and negative bank pattern '"
												+ _fngptsnames[i]
												+ "', matching items "
												+ std::to_string(
														_fingerprint_match_negatives_from)
												+ ".."
												+ std::to_string(
														_fingerprint_match_negatives_to)
												+ ", threshold="
												+ std::to_string(
														_matching_distance_negatives_max)
												+ ", match distance is "
												+ std::to_string(_matchdistance)
												+ "  "
												+ ((_matchdistance
														<= _matching_distance_negatives_max) ?
														"* individual match *" :
														"* no individual match *");
								LOG(LOG_INFO, _lmessage);
							}

						}
					}

				}

				// final evaluation using evaluation logic

				// prepare match comment
				_match_comment =
						"***Final match raised for measurements pattern no "
								+ std::to_string(_patternid) + " at " + _p1
								+ "\n***";

				// default is nonmatch
				_ismatch = 0;
				_matchdistance_out = -1;  // default is "no match"
				_contivalue = 0; // default is "no suspection"

				switch (_matches_evaluation_logic) {

				// If _matches_evaluation_logic = 0, no fingerprints matching is provided at all
				case 0:
					_ismatch = 1; // pure "alarm_noisereject + pattern related delay" like
					_contivalue = 1;
					_match_comment =
							_match_comment
									+ "Logic 0 (pure alarm_noisereject-like without bank patterns matching)";
					break;

					// If _matches_evaluation_logic = 1, not matching any negative pattern causes to raise match
				case 1:
					// _contivalue set even when not match
					_contivalue = _matchdistance_neg_min;

					// match logic
					if (_matchneg_count == 0) {
						_ismatch = 1;  // no negative pattern matched
						_matchdistance_out = _matchdistance_neg_min;
						_match_comment =
								_match_comment
										+ "Logic 1 (final match raised because no individual match for negative bank patterns raised)";
					}
					break;

					// If _matches_evaluation_logic = 2, matching any positive pattern causes raise match
				case 2:
					// _contivalue set even when not match
					_contivalue = 1 - _matchdistance_pos_min;

					// match logic
					if (_matchpos_count) {
						_ismatch = 1;  // some new pattern
						_matchdistance_out = _matchdistance_pos_min;
						_match_comment =
								_match_comment
										+ "Logic 2 (final match raised because bank pattern '"
										+ _matchtestposname
										+ "' raised an individual match)";
					}
					break;

					// If _matches_evaluation_logic = 3, not matching any negative patterns and matching any positive
				case 3:

					// _contivalue set even when not match
					_contivalue = _matchdistance_neg_min;
					if (_matchdistance_pos_min < _matchdistance_neg_min)
						_contivalue = _matchdistance_pos_min;
					_contivalue = 1 - _contivalue;

					// match logic
					if (_matchneg_count == 0 && _matchpos_count > 0) {
						_ismatch = 1;  // some new pattern

						// minimum distance from positives or negatives
						_matchdistance_out = _matchdistance_neg_min;
						if (_matchdistance_pos_min < _matchdistance_out)
							_matchdistance_out = _matchdistance_pos_min;

						_match_comment =
								_match_comment
										+ "Logic 3 (final match raised because no negative bank pattern individual match raised and positive bank pattern '"
										+ _matchtestposname
										+ "' raised an individual match)";
					}
					break;

					// If _matches_evaluation_logic = 4, matching all positive patterns
				case 4:
					// _contivalue set even when not match
					_contivalue = 1 - _matchdistance_pos_min;

					// match logic
					if (_matchpos_count) {
						_ismatch = 1;  // some new pattern
						_matchdistance_out = _matchdistance_pos_min;
						_match_comment =
								_match_comment
										+ "Logic 4 (final match raised because "
										+ std::to_string(_matchpos_count)
										+ " positive bank pattern(s) raised individual match)";
					}
					break;

				default: {
					DLOG(LOG_ERROR,
							"\nError\nUnknown evaluation logic "
									+ std::to_string(_matches_evaluation_logic)
									+ "\nExiting");
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
					return 1;
#else
					{
						//helper code for ProcessGuard:   v[0] = -2.0; pushResult(v);

					}
#endif
				}

				} // of matches evaluation logic switch

				// debug
				// std::cout << "(debug) _generate_fingerprints: " << _generate_fingerprints << std::endl;
				// std::cout << "(debug) _matchpos_count: " << _matchpos_count << std::endl;
				// std::cout << "(debug) _matchneg_count: " << _matchneg_count << std::endl;

				// output to file, if not forbidden by generate_fingerprint logic
				// debug
				// std::cout << "(debug) _curtime.tv_sec: " << _curtime.tv_sec << std::endl;
				// std::cout << "(debug) _genpattern_time.tv_sec: " << _genpattern_time.tv_sec << std::endl;
				// std::cout << "(debug) genpattern_count: " << genpattern_count << std::endl;
				// std::cout << "(debug) _matchpos_count: " << _matchpos_count << std::endl;
				// std::cout << "(debug) _matchneg_count: " << _matchneg_count << std::endl;

				if (_generate_fingerprints == 1
						|| (_generate_fingerprints == 2 && !_ismatch)) {

					//verify generating fingerprints throttle and shift time, if time limit passed
					if ((_curtime.tv_sec - _genpattern_time.tv_sec) > 60 * 60) {
						_genpattern_time = _curtime;
						_genpattern_count = 0;
					}

					if (_genpattern_hour_limit == 0
							|| (_genpattern_count < _genpattern_hour_limit))

							{

						//increment counter of generated fingerprints
						_genpattern_count++;

						// find leading zeros
						_patzeros = 0;
						if (_patternid < 10)
							_patzeros = 3;
						else if (_patternid < 100)
							_patzeros = 2;
						else if (_patternid < 10)
							_patzeros = 1;

						std::ofstream outputfile;

						// debug
						//std::cout << "(debug) _patternid: " << _patternid << std::endl;
						//std::cout << "(debug) _p1: " << _p1 << std::endl;
						//std::cout << "(debug) _wavelet_function: " << _wavelet_function << std::endl;

						// build pattern filename (incl. directory)
						_filenam = "w_" + std::string(_patzeros, '0')
								+ std::to_string(_patternid) + "_" + _p1;

						//amend filename (mainly for windows) - substitute certain characters by '_' or similar
						while (_filenam.find(":") != std::string::npos)
							_filenam = _filenam.replace(_filenam.find(":"), 1,
									"_");
						while (_filenam.find("-") != std::string::npos)
							_filenam = _filenam.replace(_filenam.find("-"), 1,
									"_");
						while (_filenam.find(".") != std::string::npos)
							_filenam = _filenam.replace(_filenam.find("."), 1,
									"_");
						while (_filenam.find(" ") != std::string::npos)
							_filenam = _filenam.replace(_filenam.find(" "), 1,
									"_");

						// add directory and suffix for fingerprints filename
						_filenam = _filenam +
						// add suffix .fpr and fingerprint parameter id
								".fpr" + std::to_string(_wavelet_function) +
								// add fingerprint length
								"_len" + std::to_string(_fingerprint_n);
						// debug
						//std::cout << "(debug) filename of wavelets: " << _filenam << std::endl;

						outputfile.open(_fingerprints_directory + _filenam);
						for (int i = 0; i < _fingerprint_n; i++) {
							outputfile << std::fixed << _vw[i] << std::endl;
						}
						outputfile.close();

						if (_debug_level) {
							_lmessage = "*Fingerprint saved to file: " + _filenam;
							LOG(LOG_INFO, _lmessage);
						}

					}

					else {
						//genpattern limit reached

						if (_debug_level) {
							_lmessage =
									"*Fingerprint generation limit within hour reached, fingerprint not saved";
							LOG(LOG_INFO, _lmessage);
						}
					}

				}

				// match raised here //////////////////////////////////////////////
				if (_ismatch && _debug_level) {
					/*****************************************************************/
					LOG(LOG_INFO, std::string(117, '*'));
					LOG(LOG_INFO, _match_comment);
					LOG(LOG_INFO, std::string(117, '*'));
					/*****************************************************************/
				}

				// clear variables for next pattern
				_seqdata.clear();
				_ispattern = 0;
			}

		} // of _ispattern

		//
		// debug
		//std::cout << "(debug) _ispattern: " << _ispattern << std::endl;

		// impute wait state, even if currently not present

		//verify if waiting period after previously detected alarm
		if (_iswait == 1) {

			// is in "wait" state

			// after entering the wait state, reset the alarm status
			_isalarm = 0;

			// debug
			// std::cout << "*(debug) alarmdiff: " << (_curtime.tv_sec - _alarmraisetime.tv_sec) * 1000000 + (_curtime.tv_usec - _alarmraisetime.tv_usec) << std::endl;

			if ((_curtime.tv_sec - _alarmraisetime.tv_sec) * 1000000 + _curtime.tv_usec - _alarmraisetime.tv_usec > _wait_state_usec)
				_iswait = 0;		// reset wait period

			// force _iswait back if still patterngeneration in process
			if (_ispattern)
				_iswait = 1;

		} else {

			// not in wait state

			if (_diff < _multiplicator_to_detect * _diffavg)
				_numthresholded = _number_of_points_to_alarm;	//reset thresholding count
			else {

				// debug
				// std::cout << "numthresholded: " << _numthresholded << std::endl;

				// if number of subsequent points is enough, raise alarm
				if (--_numthresholded == 0) {
					//	number of subsequent differences found

					// alarm raised
					_isalarm = 1;
					_alarmraisetime = _curtime;
					_iswait = 1;
					_numthresholded = _number_of_points_to_alarm;

					// pattern starts
					_patternid++;
					_ispattern = 1;
					_patterncount = _fingerprint_n;

					// push current value as the first for analysis
					if (_use_diff_value)
						_seqdata.push_back(_diffnoabs);
					else
						_seqdata.push_back(_curval);

				}
			}
		}

		// amend diffavg, use _n_amend_avgdiff

		// do not amend if in wait state of detection sequence based on _number_of_points_to_alarm
		if (_iswait == 0 && _numthresholded == _number_of_points_to_alarm)
			_diffavg = (_diffavg * (_n_amend_avgdiff - 1) + _diff)
					/ _n_amend_avgdiff;

		// if < 1, set 1
		// if (_diffavg < 1)
		//	_diffavg = 1;

		// remember current value,
		_lastval = _curval;

		// output special debug line if alarm detected
		if (_debug_level && _isalarm) {
			_lmessage = "*Alarm detected at " + _p1
					+ ", collecting measurements pattern "
					+ std::to_string(_patternid);
			LOG(LOG_INFO, _lmessage);
		}

		// outputvalue is either _contivalue or _matchdistance_output

		_outputvalue = _contivalue;
		if (_matchdistance_to_output)
			_outputvalue = _matchdistance_out;

		// output debug line
		if (_debug_level > 1) {
			_lmessage = std::to_string(_lineid) + ";" + _p1 + ";"
					+ std::to_string(_curval) + ";" + std::to_string(_diffnoabs)
					+ ";" + std::to_string(_diffavg) + ";"
					+ (_numthresholded == _number_of_points_to_alarm ? "0" : "1")
					+ ";" + std::to_string(_isalarm) + ";"
					+ std::to_string(_iswait) + ";"
					+ std::to_string(_ispattern ? _patternid : 0) + ";"
					+ std::to_string(_ismatch) + ";"
					+ std::to_string(_matchdistance_out) + ";"
					+ std::to_string(_contivalue) + ";"
					+ std::to_string(_outputvalue);

			LOG(LOG_INFO, _lmessage);
		}

		//////////////////////////////////////////////////////////////////////////////////
		// put _outputvalue to output
		if (_debug_level && _matchdistance_out != -1) {

			//debug
			//std::cout << "(debug) _contivalue: " << _contivalue << std::endl;
			//std::cout << "(debug) _matchdistance__out: " << _matchdistance_out << std::endl;

			_lmessage = "*Related outputvalue pushed to output: "
					+ std::to_string(_outputvalue);
			LOG(LOG_INFO, _lmessage);
		}

		// 				output value here				//

		// reset pattern match incl. output
		_ismatch = 0;
		_matchdistance_out = -1;
		_contivalue = 0;

	} // of input values cycle while

	return 0;
}

/*****************************************************************************
 * functions definitions
 *****************************************************************************/

// trim string
std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(' ');
	if (std::string::npos == first) {
		return str;
	}
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}

// standalone only LOG and DLOG functions
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION

void LOG(int type = 0, std::string message = "") {
	std::cout << message << std::endl;
}
void DLOG(int type = 0, std::string message = "") {
	std::cerr << message << std::endl;
}

#endif

// eucleidian distance of two vectors
// normalized result always between 0 and 1
// 0 means "same", 1 means "completely distant"
double _eucl_dist(double * v1, double * v2, int from, int to, int flength,
		int distance_calculation) {

	double cc = 0.0;
	double xx = 0.0;
	double yy = 0.0;

	int clength = log2(flength);
	std::vector<double> a1, a2;
	double acavgv1, acavgv2;
	int ifrom, ito;

	//std::cout <<  "(debug) clength: " << clength << std::endl;

	switch (distance_calculation) {

	case 2:

		for (int i = 0; i <= clength; i++) {

			// find from and to indexes
			ifrom = (i == 0 ? 0 : pow(2, i - 1));
			ito = pow(2, i) - 1;

			//debug
			// std::cout << "(debug) ifrom,ito: " << ifrom << "," << ito << std::endl;

			// calculate only if from and to fully matches full range of the level
			if (ifrom >= from && ito <= to) {
				acavgv1 = acavgv2 = 0;
				for (int j = ifrom; j <= ito; j++) {
					acavgv1 += v1[j] / (ito - ifrom + 1);
					acavgv2 += v2[j] / (ito - ifrom + 1);

					// debug
					//std::cout << "(debug) input values at j=" << j << ": " << v1[j] << "," << v2[j] << std::endl;
				}

				// debug
				//std::cout << "(debug) average value at i=" << i << ": " << acavgv1 << "," << acavgv2 << std::endl << std::endl;

				a1.push_back(acavgv1);
				a2.push_back(acavgv2);
			}

		}

		// debug
		// std::cout << "(debug) number of averaged values: " << a1.size()  << std::endl;

		for (unsigned int i = 0; i < a1.size(); i++) {
			//debug
			//std::cout << "(debug) a1,a2["<< i <<"]: " << a1[i] << "," << a2[i] << std::endl;

			// !!! to be enhanced here: more rounding prone calculations is desired
			cc += pow((a1[i] - a2[i]), 2);
			xx += pow(a1[i], 2);
			yy += pow(a2[i], 2);
		}

		//debug
		// std::cout <<  "(debug) flength: " << flength << std::endl;
		//	std::cout << "(debug) xx: " << xx << std::endl;
		//	std::cout << "(debug) yy: " << yy << std::endl;
		// 	std::cout << "(debug) cc / (xx + yy): " << cc / (xx + yy) << std::endl;

		break;

	case 1:

		for (int i = from; i <= to; i++) {

			//debug
			// std::cout << "(debug) v1,v2["<< i <<"]: " << v1[i] << "," << v2[i] << std::endl;

			cc += pow((v1[i] - v2[i]), 2);
			xx += pow(v1[i], 2);
			yy += pow(v2[i], 2);
		}

		//debug
		// std::cout <<  "(debug) flength: " << flength << std::endl;
		//	std::cout << "(debug) xx: " << xx << std::endl;
		//	std::cout << "(debug) yy: " << yy << std::endl;
		// 	std::cout << "(debug) cc / (xx + yy): " << cc / (xx + yy) << std::endl;

		break;

	default:
		//debug
		//std::cout <<  "(debug) distance_calculation: " << distance_calculation << std::endl;

		DLOG(LOG_ERROR, "\ndistance_calculation argument internal error");
		break;

	}

	cc = cc / (xx + yy);

	// amend if exceeds because of truncation error:
	if (cc > 1)
		cc = 1;
	if (cc < 0)
		cc = 0;

	return cc;  // returns normalized 0..1

}

