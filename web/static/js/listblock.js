/* eslint-env jquery */
(function( $ ){
    $.fn.listblock = function(options) {
        var defaults = {};
        var settings = $.extend( {}, defaults, options);

        return this.each(function() {
            var $orig = $(this);

            function rawValToItems(x) {
                return JSON.parse(x);
            }

            function itemsToRawVal(x) {
                return JSON.stringify(x.map(function(e) {return itemToRaw(e);})); 
            }

            function rawToItem(x) {
                if (x) {
                    return {value: x};
                } else {
                    return null;
                }
            }

            function itemToRaw(x) {
                return x.value;
            }

            function itemToHTML(x) {
                return x.value;
            }

            function setItemVal() {
                var rawVal = itemsToRawVal(items);
                if ($orig.is("select")) {
                    $orig.empty();
                    if ($orig.is("[multiple]")) {
                        var keys = items.map(function(x) {return itemToRaw(x);});
                        keys.forEach(function(x) {
                            $orig.append($("<option>").text(x));
                        });
                        $orig.val(keys);
                    } else {
                        $orig.append($("<option>").text(rawVal));
                        $orig.val(rawVal);
                    }
                } else {
                    $orig.val(rawVal);
                }
            }

            function addListElement(item, show) {
                var $item = $("<li>").html(itemToHTML(item));
                if (show) {
                    $item.addClass("show");
                } else {
                    setTimeout(function() {
                        $item.addClass("show");
                    }, 10);
                }
                var $del = $("<a>", {href: "#", class:"pull-right"}).append($("<span>", 
                    {class:"glyphicon glyphicon-minus-sign", "aria-hidden": "true"}));
                $del.click(function(e) {
                    removeListElement(item, $item);
                    e.preventDefault();
                });
                $item.append($del);
                $list.append($item);
                item._ele = $item;
                items.push(item);
                setItemVal();
                return $item;
            }

            function removeListElement(item, $item) {
                var idx = items.indexOf(item);
                if(idx>-1) {
                    items.splice(idx, 1);
                }
                setItemVal();
                $item.removeClass("show");
                setTimeout(function() {
                    $item.remove();
                }, 500);
            }

            function highlightListElement(item, $item) {
                $item = item._ele;
                $item.addClass("focus"); 
                setTimeout(function() {
                    $item.removeClass("focus");
                }, 700);
            }

            var $list = $("<ul>");
            var items = [];
            var initItems =  settings.items || rawValToItems($orig.val());
            initItems.forEach(function(x) {
                addListElement(rawToItem(x), true);
            });
        
            var $input = $("<input>", {type:"text"});
            $input.typeahead({hint: true},
                {name: "contacts", source: function(query, sync) {
                    sync( ["hello","goodbye"]);
                }}
            );

            $input.on("keypress", function(e) {
                if (e.key=="Enter") {
                    e.preventDefault();
                    $addbutton.trigger("click");
                    //console.log("error");
                }
            });

            var $addbutton = $("<button>").append($("<span>", {class: "glyphicon glyphicon-plus-sign"}));
            $addbutton.click(function(e) {
                e.preventDefault();
                var item = rawToItem($input.val());
                if (item) {
                    var existing = items.find(function(x) {return x.value==item.value;});
                    if (!existing) {
                        addListElement(item);
                    } else {
                        highlightListElement(existing);
                    }
                }
                $input.val("");
            });
            var $container = $("<div>", {class: "listblock"});
            $container.append($input).append($addbutton).append($list).insertAfter($orig);

            return $orig.hide();
        });
    };
})(jQuery);
