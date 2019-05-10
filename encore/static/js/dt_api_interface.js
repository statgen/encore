/* global $ */
/* exported encoreApi */
// inspired by https://datatables.net/examples/server_side/pipeline.html

function encoreApi(opts) {
    if (typeof(opts) === "string") {
        opts = { url: opts };
    }
    var conf = $.extend( {
        url: "",
        serverPageSize: 100,
    }, opts);
    var cacheLastRequest = null;
    var cacheLastJson = null;
    var cache = {lower: -1, upper: -1};
    var serverPageSize = conf.serverPageSize;

    function json_differ(a, b) {
        return JSON.stringify( a ) !== JSON.stringify( b );
    }

    return function(request, drawCallback, settings) {
        var apiOpts = { echo: request.draw };
        if (request.start > 0) {
            apiOpts.offset = request.start;
            apiOpts.limit = serverPageSize;
        }
        var order_by = request.order.map(function(x) {
            return ((x.dir=="desc")?"-":"") + request.columns[x.column].data;
        }).join(",");
        apiOpts.order_by = order_by;
        if (request.search && request.search.value) {
            apiOpts.q = request.search.value;
        }

        var needsFetching = false;
        if ( cache.lower < 0 || request.start < cache.lower ||
            (request.start + request.length) > cache.upper ) {

            needsFetching = true;

        } else if (json_differ(apiOpts.order_by, cacheLastRequest.order_by ) ||
                   json_differ( apiOpts.q, cacheLastRequest.q ) )  {

            needsFetching = true;
        }

        cacheLastRequest = $.extend( true, {}, apiOpts );

        if (needsFetching) {
            settings.jqXHR = $.ajax( {
                "type": "GET",
                "url": conf.url,
                "data": apiOpts,
                "dataType": "json",
                "cache": false,
                "success": function(json) {
                    if (json.header && json.header.pages) {
                        //paged response
                        json.draw = json.header.echo;
                        json.recordsTotal = json.header.total_count;
                        json.recordsFiltered = json.header.filtered_count;

                        cacheLastJson = $.extend(true, {}, json);
                        serverPageSize = (json.header.limit || json.recordsTotal);
                        cache.lower = json.header.offset || 0;
                        cache.upper = cache.lower + serverPageSize;

                        json.data.splice( 0, request.start-cache.lower );
                        json.data.splice( request.length, json.data.length );
                        drawCallback(json);
                    } else {
                        //non-paged response, handle locally	
                        if (!json.header) {
                            json = { data: json };
                        }
                        cache.lower = 0;
                        cache.upper = json.data.length;

                        settings.oFeatures.bServerSide = false;
                        drawCallback(json);
                    }
                }
            });
        } else {
            var json = $.extend( true, {}, cacheLastJson );
            json.draw = request.draw; // Update the echo for each response
            json.data.splice( 0, request.start-cache.lower );
            json.data.splice( request.length, json.data.length );

            drawCallback(json);
        }
    };
}
