// Examples taken from a rejected transaction from FD Datawire + Omaha.

#define RESPONSE "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\\n" \
                 "<Response Version=\"3\" xmlns=\"http://securetransport.dw/rcservice/xml\">\\n" \
                 " <RespClientID>\\n" \
                 "  <DID>00011549362935938987</DID>\\n" \
                 "  <ClientRef>1V</ClientRef>\\n" \
                 " </RespClientID>\\n" \
                 " <Status StatusCode=\"OK\"></Status>\\n" \
                 " <TransactionResponse>\\n" \
                 "  <ReturnCode>000</ReturnCode>\\n" \
                 "  <Payload Encoding=\"xml_escape\">" \
                   "&lt;RejectResponse&gt;" \
                     "&lt;RespGrp&gt;" \
                       "&lt;RespCode&gt;" "001" "&lt;/RespCode&gt;" \
                       "&lt;ErrorData&gt;" "NO ERROR PROVIDED" "&lt;/ErrorData&gt;" \
                     "&lt;/RespGrp&gt;" \
                     "&lt;![CDATA[fDAyKjExLlJDVFNUMDAwMDAwMDU0NCMwMDAxMTU0OTM2MjkzNTkzODk4N3wxQ0EwMXwxRjMwMDB8MUYxfDFGOXwxQzJ8MUMxLjAwfDFDfDFDMDAwMDB8MUM=]]&gt;" \
                   "&lt;/RejectResponse&gt;" \
                   "&lt;/GMF&gt;"  /* wait, GMF?? */ \
                 "</Payload>\\n" \
                 " </TransactionResponse>\\n" \
                 "</Response>"

#define PAYLOAD "<RejectResponse>" \
                  "<RespGrp>" \
                    "<RespCode>001</RespCode>" \
                    "<ErrorData>NO ERROR PROVIDED</ErrorData>" \
                  "</RespGrp>" \
                  "<![CDATA[fDAyKjExLlJDVFNUMDAwMDAwMDU0NCMwMDAxMTU0OTM2MjkzNTkzODk4N3wxQ0EwMXwxRjMwMDB8MUYxfDFGOXwxQzJ8MUMxLjAwfDFDfDFDMDAwMDB8MUM=]]>" \
                "</RejectResponse>" \
                "</GMF>" // lolwut

#include <libxml/parser.h>
#include <libxml/tree.h>
#include "bindings.h"

#define DUMP "(function(ele)\n"                                              \
                "function xmldump(node)\n"                                   \
                  "if type(node) ~= 'table' then return tostring(node) end\n"\
                  "local result = '<' .. node._name \n"                      \
                  "for key, value in pairs(node) do\n"                       \
                    "if key ~= '_name' and type(key) ~= 'number' then\n"     \
                      "result = result .. ' ' .. key .. '=' .. \n"           \
                      "         '\"' .. value .. '\"'\n"                     \
                    "end\n"                                                  \
                  "end\n"                                                    \
                  "if #node == 0 then result = result .. ' />'\n"            \
                  "else\n"                                                   \
                    "result = result .. '>'\n"                               \
                    "for i, child in ipairs(node) do\n"                      \
                      "result = result .. xmldump(child)\n"                  \
                    "end\n"                                                  \
                    "result = result .. '</' .. node._name .. '>'\n"         \
                  "end\n"                                                    \
                  "return result\n"                                          \
                "end\n"                                                      \
                "return xmldump(ele)\n"                                      \
             "end)(doc)"

#define TEST_SINGLE_EMPTY_ELEMENT "xml = require('lxml'); doc = xml.parse('<a/>');                     print(" DUMP "); assert(type(doc) == 'table')"
#define TEST_SINGLE_ELEMENT_ATTRS "xml = require('lxml'); doc = xml.parse('<a b=\"one\" c=\"two\"/>'); print(" DUMP "); assert(doc.b == 'one'); assert(doc.c == 'two')"
#define TEST_SINGLE_CHILD         "xml = require('lxml'); doc = xml.parse('<a><b/></a>');              print(" DUMP "); assert(#doc == 1); assert(type(doc[1]) == 'table')"
#define TEST_TEXT_CHILD           "xml = require('lxml'); doc = xml.parse('<a>lorem &amp; ipsum</a>'); print(" DUMP "); assert(#doc == 1); assert(doc[1] == 'lorem & ipsum')"
#define TEST_SEVERAL_CHILDREN     "xml = require('lxml'); doc = xml.parse('<a><b/><b/><c/><c/></a>');  print(" DUMP "); assert(#doc == 4); assert(doc[1]._name == 'b'); assert(doc[2]._name == 'b'); assert(doc[3]._name == 'c'); assert(doc[4]._name == 'c')"
#define TEST_DATAWIRE_RESPONSE    "xml = require('lxml'); doc = xml.parse('" RESPONSE "');             print(" DUMP "); assert(doc[3][2][1] == '" PAYLOAD "')"
#define TEST_DATAWIRE_PAYLOAD     "xml = require('lxml'); doc = xml.parse('" PAYLOAD  "');             print(" DUMP "); assert(doc[2] == 'fDAyKjExLlJDVFNUMDAwMDAwMDU0NCMwMDAxMTU0OTM2MjkzNTkzODk4N3wxQ0EwMXwxRjMwMDB8MUYxfDFGOXwxQzJ8MUMxLjAwfDFDfDFDMDAwMDB8MUM=')"

int main(int argc, char **argv) {
  int err = 0;
  xmlInitParser();
  if ((err = lua_run_script(TEST_SINGLE_EMPTY_ELEMENT))) goto done;
  if ((err = lua_run_script(TEST_SINGLE_ELEMENT_ATTRS))) goto done;
  if ((err = lua_run_script(TEST_SINGLE_CHILD)))         goto done;
  if ((err = lua_run_script(TEST_TEXT_CHILD)))           goto done;
  if ((err = lua_run_script(TEST_SEVERAL_CHILDREN)))     goto done;
  if ((err = lua_run_script(TEST_DATAWIRE_RESPONSE)))    goto done;
  if ((err = lua_run_script(TEST_DATAWIRE_PAYLOAD)))     goto done;

done:
  xmlCleanupParser();
  return err;
}
