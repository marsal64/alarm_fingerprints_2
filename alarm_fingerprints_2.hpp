/*
 * alarm_fingerprints.hpp
 *
 *
 *      Author: Martin Saly
 */

#ifndef ALARM_FINGERPRINTS_HPP_
#define ALARM_FINGERPRINTS_HPP_

#include "wavelet_ms.hpp"

// standalone version helper //////////////////////////
// comment the following line if not standalone version
#define ALARM_FINGERPRINTS_STANDALONE_VERSION
///////////////////////////////////////////////////////

// parametrization start ///////////////////////////////////////////////////////////////////

// Print help
int _print_help;

// Sampling parameter: uses each nth input value for evaluation
// If _sample_each == 1, takes every value (no sampling)
int _sample_each;

// Value of the initial absolute difference between subsequent points representing noise
// The value should be set based on real average value
// This value is used as the initial value for avgdiff variable
double _initial_avg_diff; // initial value of average noise difference

// Smoothing constant for subsequent diffavg amendments, must be >= 1
// higher value means lower sensitivity to average diff value change
int _n_amend_avgdiff;

// How many subsequent points need to have greater difference between them
// than avgdiff * _multiplicator_to_detect to raise alarm
int _number_of_points_to_alarm;

// Multiplicator to multiplicate diffavg value to set the threshold for alarm detection
double _multiplicator_to_detect;

// How many microseconds must elapse between detected alarm to detect subsequent alarm
// e.g. 5 secs = 5000000
int _wait_state_usec;

// Vectors comparison type
// 1 - vector items normalized eucleidian distance
// 2 - eucleidian distance of the averages of vectors levels
int _distance_calculation_type;

// Pattern length for fingerprints gen: how many measurements (or sampled measurments) taken (starting from alarm detection)
// for fingerprints generation
// Should be power of 2 value, if not, will be truncated internally to nearest lower x**2
int _fingerprint_length;

// Matching from fingerprint index start (0 means shift)
// If fingerprint_match_..._from  > 1, highpass filter is applied for matching
int _fingerprint_match_positives_from;
int _fingerprint_match_negatives_from;

// Matching to fingeprint index end. Must be at max fingerprint_length - 1
// If fingerprint_match_..._to < fingerprint_length - 1, lowpass filter is applied for matching
int _fingerprint_match_positives_to;
int _fingerprint_match_negatives_to;

// Default function for wavelet method (pointer to function)
// Daubechie level of function, e.g. 12 = "daub12" wavelet
int _wavelet_function;
double * (*_wav_func)(int, double *); // default pointer to function

// If fingerprints should be generated to files
// 0 used for matching run
// 1 used for learning/generating new patterns
int _generate_fingerprints;

// Maximum distance to match for any positive pattern
// Positive patterns identified as p_xxxxxxx.fpryyyy
//   x,y are any chars. xxxxxxxx forms pattern name
double _matching_distance_positives_max;

// Maximum distance to match for any negative pattern
// Negative patterns identified as n_xxxxxxx.fpryyyy
//   x,y are any chars. xxxxxxxx forms pattern name
double _matching_distance_negatives_max;

// If matches_evaluation_logic = 0, no fingerprints matching is provided at all
//    (suitable for fast but unsafe alarm_noisereject like run)
// If matches_evaluation_logic = 1, not matching any negative pattern causes to raise match
//    (suitable for eliminating all known alarms/matches and detect new)
// If matches_evaluation_logic = 2, matching any positive pattern causes raise match
//    (suitable for matching only known alarms/matches)
// If matches_evaluation_logic = 3, not matching any negative patterns and matching any positive pattern causes to raise match
//    (suitable for "strict" alarms/matches - may indicate tuning need)
// If matches_evaluation_logic = 4, same as 2 but all positive patterns matched
int _matches_evaluation_logic;

// Skips line if contains certain character
// In default case skips default header line where "m" occurs at each line
std::string _skip_if_contains;

// If use_diff_value = 0, uses meas value for wavelets calculation, else uses diff value
int _use_diff_value;

// Directory for fingerprints
// Same directory from which fingerprints are read and stored (if _generate_fingerprints=1)
// Default is current
// Without trailing '/'
std::string _fingerprints_directory;

// Level how debug is generated
// 1 - commenting debug info
// 2 - same as 1 and additionally:
//       detailed loaded fingerprints values
//       detailed per line output data
int _debug_level;

// If to output should be generated contivalue or matchdistance_out
// 0 - contivalue
// 1 - matchdistance_out
int _matchdistance_to_output;

// number of fingerprints to generate within one hour
// 0 means unlimited
int _genpattern_hour_limit;

// Non-parameters definitions
// Maximum number of fingerprints to load
#define MAX_FINGEPRINTS_TO_LOAD 500

// parametrization end ///////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////
// Define working variables

// working value of average difference for alarm_noisereject part
float _diffavg;

int _fingerprint_n;
// variables for patterns generating/matching
std::vector<double> _seqdata;
double* _dw;
double* _vw; 	// wavelets results
int _patzeros;  // number of zeros before patternid
int _cursample; // init sample count

// variables for read data and parsing
std::string _lineread, _p1, _p2;

// helper variable for string char conversion and other standalone only
char* c;
// helper tm structure for conversion
struct tm t = { 0 };  // Initialize to 0
// helper working variables for timestamp parsing
int d, m, y, h, mi, s, ms;
// helper position variable
unsigned long int pos;
// helper for parsing
int co;


// variables related to alarm
int _isalarm;
int _iswait;
int _numthresholded;

// variables for alarm delay calculation
struct timeval _lasttime; // large enough
struct timeval _curtime;
struct timeval _alarmraisetime;    // small enough

// generation patterns count related
struct timeval _genpattern_time; // recorded time of generating fingerprint
int _genpattern_count;

// line id
long long _lineid;

// values for calculations (mind type)
double _curval, _lastval; // current and last value
double _diff; // difference from previous value, absolute value
double _diffnoabs; // noabsvalue

// patterns related variables
int _patternid;
int _ispattern; 	// status variable for pattern tracking
int _patterncount; 	// counts current pattern length
std::string _filenam;	// work wariable for pattern filename
int _ismatch;		// variable indicating final match
int _numberfps; // "real" counter of fingerprints

// variables for patterns matching logic
int _pos_count, _matchpos_count, _neg_count, _matchneg_count;
std::string _matchtestposname, _matchtesnegname;
double _matchdistance, _matchdistance_out;
double _matchdistance_pos_min, _matchdistance_neg_min;
double _contivalue;
double _outputvalue;
std::string _match_comment;

//define vector for fingerprints
std::vector<std::vector<double> > _fngpts;

//define vector for fingeprint names
std::vector<std::string> _fngptsnames;

// initialize vector of filenames
std::vector<std::string> _patfilenames;

// variable for messaging
std::string _lmessage;

// construct list of potential filenames to _patfilenames vector
DIR *_dir;
struct dirent *_ent;
std::string _hstr;
std::regex _matchstr1;
std::regex _matchstr2;

#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
// definitions of messages
#define LOG_INFO 0
#define LOG_WARNING 3
#define LOG_ERROR 4

#endif
// function prototypes

// trim function
std::string trim(const std::string&);

// distance function
double _eucl_dist(double *, double *, int, int, int, int);

//LOG and DLOG functions, standalone only
#ifdef ALARM_FINGERPRINTS_STANDALONE_VERSION
void LOG(int, std::string);
void DLOG(int, std::string);
#endif

#endif /* ALARM_FINGERPRINTS_HPP_ */
