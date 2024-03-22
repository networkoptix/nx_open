// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <map>
#include <memory>
#include <utility>

#include <QtCore/QString>

#include "params.h"

namespace nx::network::rest {

class Handler;
struct Request;

/**
 * Hierarchical container of handlers for string paths separated on path items by '/' symbol. Each
 * path item can be a fixed, a regex, a parameter or an optional parameter item. The last item of a
 * path contains a Handler. A regex item is specified with the '^' prefix. A parameter item is
 * specified with the ':' prefix. An optional parameter item is like a parameter with addition of
 * the '?' suffix. Fixed items are other non-parameter items. During a search for a handler for the
 * specified path by findMatchedResult(), all found parameters and optional parameters are
 * stored in a returned Result::pathParams. The key of such parameter is the name of the path item
 * specified in the registered path, and the value is the string of the matched path item itself.
 * Result::validationPath is filled with the passed path items, value items are presented as
 * '{name}' to match the paths of the Open API validation schema.
 *
 * An example of a path string for the addHandler():
 * 'rest/^v[1-9]/devices/:deviceId/bookmarks/:guid?', where 'rest', 'devices' and 'bookmarks' are
 * the fixed items. `^v[1-9]` is a regex, 'deviceId' is a parameter and 'guid' is an optional
 * parameter that will be used as the keys to fill in the Result::pathParams with the values from a
 * real path passed to the findMatchedResult(). The Result::validationPath for the example is
 * 'rest/v1/devices/{deviceId}/bookmarks/{guid}', if an optional `guid` is specified, and a path is
 * started with `rest/v1`, and 'rest/v2/devices/{deviceId}/bookmarks', if an optional `guid` is not
 * specified, and a path is started with `rest/v2`.
 */
class NX_NETWORK_REST_API PathRouter
{
public:
    struct Result
    {
        Handler* handler = nullptr;
        QString validationPath;
        Params pathParams;
    };

    PathRouter();
    ~PathRouter();
    PathRouter(const PathRouter&) = delete;
    PathRouter& operator=(const PathRouter&) = delete;

    Handler* findHandlerOrThrow(Request* request, const QString& pathIgnorePrefix = {}) const;
    void addHandler(const QString& path, std::unique_ptr<Handler> handler);

    std::vector<std::pair<QString, QString>> findOverlaps(const Request& request) const;
    static QString replaceVersionWithRegex(const std::string_view& path);

private:
    class Item;
    std::unique_ptr<Item> m_root;
    std::deque<std::string> m_pathHolder;
};

} // namespace nx::network::rest
