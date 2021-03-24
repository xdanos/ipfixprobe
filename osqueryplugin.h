/**
 * \file osqueryplugin.h
 * \brief Plugin for parsing osquery traffic.
 * \author Anton Aheyeu aheyeant@fit.cvut.cz
 * \date 2021
 */

/*
 * Copyright (C) 2021 CESNET
 *
 * LICENSE TERMS
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#ifndef OSQUERYPLUGIN_H
#define OSQUERYPLUGIN_H

#include <string>
#include <poll.h>
#include <unistd.h>
#include <sys/wait.h>


#ifdef WITH_NEMEA
# include "fields.h"
#endif

#include "flowifc.h"
#include "flowcacheplugin.h"
#include "packet.h"
#include "ipfixprobe.h"

using namespace std;

#define DEFAULT_FILL_TEXT "UNDEFINED"

// OsqueryStateHandler
#define FATAL_ERROR    0b00000001 // 1;  Fatal error, cannot be fixed
#define OPEN_FD_ERROR  0b00000010 // 2;  Failed to open osquery FD
#define READ_ERROR     0b00000100 // 4;  Error while reading
#define READ_SUCCESS   0b00001000 // 8;  Data read successfully

// OsqueryRequestManager
#define BUFFER_SIZE            1024 * 20 + 1
#define READ_SIZE              1024
#define POLL_TIMEOUT           200 // millis
#define READ_FD                0
#define WRITE_FD               1
#define MAX_NUMBER_OF_ATTEMPTS 2 // Max number of osquery error correction attempts


/**
 * \brief Flow record extension header for storing parsed OSQUERY packets.
 */
struct RecordExtOSQUERY : RecordExt {
   string   program_name;
   string   username;
   string   os_name;
   uint16_t os_major;
   uint16_t os_minor;
   string   os_build;
   string   os_platform;
   string   os_platform_like;
   string   os_arch;
   string   kernel_version;
   string   system_hostname;


   RecordExtOSQUERY() : RecordExt(osquery)
   {
      program_name     = DEFAULT_FILL_TEXT;
      username         = DEFAULT_FILL_TEXT;
      os_name          = DEFAULT_FILL_TEXT;
      os_major         = 0;
      os_minor         = 0;
      os_build         = DEFAULT_FILL_TEXT;
      os_platform      = DEFAULT_FILL_TEXT;
      os_platform_like = DEFAULT_FILL_TEXT;
      os_arch          = DEFAULT_FILL_TEXT;
      kernel_version   = DEFAULT_FILL_TEXT;
      system_hostname  = DEFAULT_FILL_TEXT;
   }

   RecordExtOSQUERY(const RecordExtOSQUERY *record) : RecordExt(osquery)
   {
      program_name     = record->program_name;
      username         = record->username;
      os_name          = record->os_name;
      os_major         = record->os_major;
      os_minor         = record->os_minor;
      os_build         = record->os_build;
      os_platform      = record->os_platform;
      os_platform_like = record->os_platform_like;
      os_arch          = record->os_arch;
      kernel_version   = record->kernel_version;
      system_hostname  = record->system_hostname;
   }

   #ifdef WITH_NEMEA
   virtual void fillUnirec(ur_template_t *tmplt, void *record)
   {
      ur_set_string(tmplt, record, F_OSQUERY_PROGRAM_NAME, program_name.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_USERNAME, username.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_OS_NAME, os_name.c_str());
      ur_set(tmplt, record, F_OSQUERY_OS_MAJOR, os_major);
      ur_set(tmplt, record, F_OSQUERY_OS_MINOR, os_minor);
      ur_set_string(tmplt, record, F_OSQUERY_OS_BUILD, os_build.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_OS_PLATFORM, os_platform.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_OS_PLATFORM_LIKE, os_platform_like.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_OS_ARCH, os_arch.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_KERNEL_VERSION, kernel_version.c_str());
      ur_set_string(tmplt, record, F_OSQUERY_SYSTEM_HOSTNAME, system_hostname.c_str());
   }

   #endif // ifdef WITH_NEMEA

   virtual int fillIPFIX(uint8_t *buffer, int size)
   {
      int length, total_length = 0;

      // OSQUERY_PROGRAM_NAME
      length = program_name.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, program_name.c_str(), length);
      total_length += length + 1;

      // OSQUERY_USERNAME
      length = username.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, username.c_str(), length);
      total_length += length + 1;

      // OSQUERY_OS_NAME
      length = os_name.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, os_name.c_str(), length);
      total_length += length + 1;

      // OSQUERY_OS_MAJOR
      *(uint16_t *) (buffer + total_length) = ntohs(os_major);
      total_length += 2;

      // OSQUERY_OS_MINOR
      *(uint16_t *) (buffer + total_length) = ntohs(os_minor);
      total_length += 2;

      // OSQUERY_OS_BUILD
      length = os_build.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, os_build.c_str(), length);
      total_length += length + 1;

      // OSQUERY_OS_PLATFORM
      length = os_platform.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, os_platform.c_str(), length);
      total_length += length + 1;

      // OSQUERY_OS_PLATFORM_LIKE
      length = os_platform_like.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, os_platform_like.c_str(), length);
      total_length += length + 1;

      // OSQUERY_OS_ARCH
      length = os_arch.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, os_arch.c_str(), length);
      total_length += length + 1;

      // OSQUERY_KERNEL_VERSION
      length = kernel_version.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, kernel_version.c_str(), length);
      total_length += length + 1;

      // OSQUERY_SYSTEM_HOSTNAME
      length = system_hostname.length();
      if (total_length + length + 1 > size) {
         return -1;
      }
      buffer[total_length] = length;
      memcpy(buffer + total_length + 1, system_hostname.c_str(), length);
      total_length += length + 1;

      return total_length;
   } // fillIPFIX
};


/**
 * \brief Additional structure for handling osquery states.
 */
struct OsqueryStateHandler {
    OsqueryStateHandler() : OSQUERY_STATE(0), isSocketEventsAuditEnabled(false) {}

    bool isErrorState() const { return (OSQUERY_STATE & (FATAL_ERROR | OPEN_FD_ERROR | READ_ERROR)); }

    void setFatalError() { OSQUERY_STATE |= FATAL_ERROR; }
    bool isFatalError() const { return (OSQUERY_STATE & FATAL_ERROR); }

    void setOpenFDError() { OSQUERY_STATE |= OPEN_FD_ERROR; }
    bool isOpenFDError() const { return (OSQUERY_STATE & OPEN_FD_ERROR); }

    void setReadError() { OSQUERY_STATE |= READ_ERROR; }
    bool isReadError() const { return (OSQUERY_STATE & READ_ERROR); }

    void setReadSuccess() { OSQUERY_STATE |= READ_SUCCESS; }
    bool isReadSuccess() const { return (OSQUERY_STATE & READ_SUCCESS); }

    void setAuditEnabled(bool enabled) { isSocketEventsAuditEnabled = enabled; }
    bool isAuditEnabled() const { return isSocketEventsAuditEnabled; }

    /**
     * Reset the \p OSQUERY_STATE. Fatal and open fd errors will not be reset.
     */
    void refresh() { OSQUERY_STATE = OSQUERY_STATE & (FATAL_ERROR | OPEN_FD_ERROR); }

    /**
     * Reset the \p OSQUERY_STATE and \p isSocketEventsAuditEnabled. Fatal and open fd errors will be reset.
     */
    void reset() { OSQUERY_STATE = 0; isSocketEventsAuditEnabled = false; }

private:
    uint8_t OSQUERY_STATE;
    bool isSocketEventsAuditEnabled;
};


/**
 * \brief Additional structure for store and convert data from flow (src_ip, dst_ip, src_port, dst_port) to string.
 */
struct ConvertedFlowData {
    /**
     * Constructor for IPv4-based flow.
     * @param sourceIPv4 source IPv4 address.
     * @param destinationIPv4 destination IPv4 address.
     * @param sourcePort source port.
     * @param destinationPort destination port.
     */
    ConvertedFlowData(uint32_t sourceIPv4, uint32_t destinationIPv4, uint16_t sourcePort, uint16_t destinationPort);

    /**
 * Constructor for IPv6-based flow.
 * @param sourceIPv6 source IPv6 address.
 * @param destinationIPv6 destination IPv6 address.
 * @param sourcePort source port.
 * @param destinationPort destination port.
 */
    ConvertedFlowData(const uint8_t *sourceIPv6, const uint8_t *destinationIPv6, uint16_t sourcePort, uint16_t destinationPort);

    string src_ip;
    string dst_ip;
    string src_port;
    string dst_port;

private:
    /**
     * Converts an IPv4 numeric value to a string.
     * @param addr IPv4 address.
     * @param isSourceIP if true - source IP conversion mode, if false - destination IP conversion mode.
     */
    void convertIPv4(uint32_t addr, bool isSourceIP);

    /**
     * Converts an IPv6 numeric value to a string.
     * @param addr IPv6 address.
     * @param isSourceIP if true - source IP conversion mode, if false - destination IP conversion mode.
     */
    void convertIPv6(const uint8_t *addr, bool isSourceIP);

    /**
     * Converts the numeric port value to a string.
     * @param port
     * @param isSourcePort if true - source port conversion mode, if false - destination port conversion mode.
     */
    void convertPort(uint16_t port, bool isSourcePort);
};


/**
 * \brief Manager for communication with osquery
 */
struct OsqueryRequestManager {
   OsqueryRequestManager();

   ~OsqueryRequestManager();

   const RecordExtOSQUERY *getRecord(){ return recOsquery; }

   /**
    * Fills the record with OS values from osquery.
    */
   void readInfoAboutOS();

    /**
      * Fills the record with program values from osquery.
      * @param flowData flow data converted to string.
      * @return true if success or false.
      */
    bool readInfoAboutProgram(const ConvertedFlowData &flowData);

private:

    /**
     * Sends a request and receives a response from osquery.
     * @param query sql query according to osquery standards.
     * @param reopenFD if true - tries to reopen fd.
     * @return number of bytes read.
     */
   size_t executeQuery(const string &query, bool reopenFD = false);

    /**
     * Writes query to osquery input FD.
     * @param query sql query according to osquery standards.
     * @return true if success or false.
     */
   bool writeToOsquery(const char *query);

    /**
     * Reads data from osquery output FD.
     * \note Can change osquery state. Possible changes: READ_ERROR, READ_SUCCESS.
     * @return number of bytes read.
     */
   size_t readFromOsquery();

    /**
     * Opens osquery FD.
     * \note Can change osquery state. Possible changes: FATAL_ERROR, OPEN_FD_ERROR.
     */
   void openOsqueryFD();

   /**
    * Closes osquery FD.
    */
   void closeOsqueryFD();

    /**
     * Before reopening osquery tries to kill the previous osquery process.
     *
     * If \p useWhonangOption is true then the waitpid() function will be used
     * in non-blocking mode(can be called before the process is ready to close,
     * the process will remain in a zombie state). At the end of the application,
     * a zombie process may remain, it will be killed when the application is closed.
     * Else if \p useWhonangOption is false then the waitpid() function will be used
     * in blocking mode(will wait for the process to complete). Will kill all unnecessary
     * processes, but will block the application until the killed process is finished.
     *
     * @param useWhonangOption if true will be used non-blocking mode.
     */
   void killPreviousProcesses(bool useWhonangOption = true) const;

    /**
      * Checks if audit socket event mode is enabled.
      */
    void checkAuditMode();

    /**
     * Tries to get the process id from table "process_open_sockets".
     * In case of failure and the socket audit mode is enabled, it
     * tries to get the pid of the process from table "socket_events".
     * @param[out] pid      process id.
     * @param[in]  flowData flow data converted to string.
     * @return true true if success or false.
     */
    bool getPID(string &pid, const ConvertedFlowData &flowData);

    /**
      * Parses json string with only one element.
      * @param[in]  singleKey    key.
      * @param[out] singleValue  value.
      * @return true if success or false.
      */
    bool parseJsonSingleItem(const string &singleKey, string &singleValue);

   /**
    * Parses json by template.
    * @return true if success or false.
    */
   bool parseJsonOSVersion();

   /**
    * Parses json by template.
    * @return true if success or false.
    */
   bool parseJsonAboutProgram();

    /**
     * From position \p from tries to find two strings between quotes ["key":"value"].
     * @param[in]  from  start position in the buffer.
     * @param[out] key   value for the "key" parsing result.
     * @param[out] value value for the "value" parsing result.
     * @return the position where the text search ended, 0 if end of json row or -1 if end of buffer.
     */
   int parseJsonItem(int from, string &key, string &value);

    /**
     * From position \p from tries to find string between quotes.
     * @param[in]  from start position in the buffer.
     * @param[out] str  value for the parsing result.
     * @return the position where the text search ended, 0 if end of json row or -1 if end of buffer.
     */
   int parseString(int from, string &str);

    /**
     * Create a new process for connecting FD.
     * @param[in]  command  command to execute in sh.
     * @param[out] inFD     input FD.
     * @param[out] outFD    output FD.
     * @return pid of the new process.
     */
   pid_t popen2(const char *command, int *inFD, int *outFD) const;

   /**
    * Sets the first byte in the buffer to zero
    */
   void clearBuffer(){ buffer[0] = 0; }

    /**
  * Tries to find the position in the buffer where the json data starts.
  * @return position number or -1 if position was not found.
  */
    int getPositionForParseJson();

   int                 inputFD;
   int                 outputFD;
   char *              buffer;
   pollfd *            pfd;
   RecordExtOSQUERY *  recOsquery;
   bool                isFDOpened;
   int                 numberOfAttempts;
   pid_t               osqueryProcessId;

   OsqueryStateHandler handler;
};


/**
 * \brief Flow cache plugin for parsing OSQUERY packets.
 */
class OSQUERYPlugin : public FlowCachePlugin
{
public:
   OSQUERYPlugin(const options_t &module_options);
   OSQUERYPlugin(const options_t &module_options, vector<plugin_opt> plugin_options);
   void init();
   int pre_create(Packet &pkt);
   int post_create(Flow &rec, const Packet &pkt);
   int pre_update(Flow &rec, Packet &pkt);
   int post_update(Flow &rec, const Packet &pkt);
   void pre_export(Flow &rec);
   void finish();
   const char **get_ipfix_string();
   string get_unirec_field_string();
   bool include_basic_flow_fields();

private:
   OsqueryRequestManager *manager;
   int numberOfSuccessfullyRequests;
   bool print_stats; /**< Print stats when flow cache finish. */
};

#endif // ifndef OSQUERYPLUGIN_H
