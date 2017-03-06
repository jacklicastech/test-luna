#define LUA_LIB

#include <lua.h>
#include <lauxlib.h>

#include <czmq.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "services/logger.h"

#ifdef LIBXML_TREE_ENABLED

static int construct_lua_dom(lua_State *L, xmlNode *a_node, int depth) {
  xmlNode *cur_node = NULL;
  int nodes = 0;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      nodes++;
      lua_newtable(L);

      lua_pushstring(L, (const char *) cur_node->name);
      lua_setfield(L, -2, "_name");

      xmlAttr* attribute = cur_node->properties;
      while(attribute)
      {
        xmlChar* value = xmlNodeListGetString(cur_node->doc, attribute->children, 1);
        lua_pushstring(L, (const char *) value);
        lua_setfield(L, -2, (const char *) attribute->name);
        xmlFree(value); 
        attribute = attribute->next;
      }

      int count = construct_lua_dom(L, cur_node->children, depth + 1);
      while (count > 0) {
        lua_rawseti(L, -(1 + count), count);
        count--;
      }
    } else if (cur_node->type == XML_TEXT_NODE || cur_node->type == XML_CDATA_SECTION_NODE) {
      xmlChar *text = xmlNodeGetContent(cur_node);
      xmlChar *c;
      int white = 1;
      for (c = text; *c; c++) {
        if (!isspace(*c)) { white = 0; break; }
      }
      if (!white) {
        lua_pushstring(L, (const char *) text);
        nodes++;
      }
      xmlFree(text);
    }
  }

  return nodes;
}

/*
 * Parses an XML document into a nested table structure.
 * 
 * Examples:
 * 
 *     xml = require("lxml")
 *     a = xml.parse("<a one='two'><b/><b/></a>")
 *     print(a.two, a[0], a[1])
 */
static int xml_parse(lua_State *L) {
  xmlDoc *doc = NULL;
  xmlNode *root_element = NULL;
  LIBXML_TEST_VERSION

  lua_newtable(L);

  const char *xmlstr = luaL_checkstring(L, 1);
  doc = xmlReadMemory(xmlstr, strlen(xmlstr),
                      "noname.xml", NULL, XML_PARSE_RECOVER | XML_PARSE_NONET);
  root_element = xmlDocGetRootElement(doc);
  int num = construct_lua_dom(L, root_element, 0);
  xmlFreeDoc(doc);

  return num;
}

#else // LIBXML_TREE_ENABLED

static int xml_parse(lua_State *L) {
  return luaL_error(L, "xml: tree API not supported by this build of libxml2");
}

#endif // LIBXML_TREE_ENABLED

static const luaL_Reg xml_methods[] = {
  {"parse", xml_parse},
  {NULL,    NULL}
};

LUALIB_API int luaopen_xml(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, xml_methods, 0);

  return 1;
}

int init_xml_lua(lua_State *L) {
  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package

  // Store CTOS module definition at preload.CTOS
  lua_pushcfunction(L, luaopen_xml);
  lua_setfield(L, -2, "lxml");

  return 0;
}

void shutdown_xml_lua(lua_State *L) {
  (void) L;
}
