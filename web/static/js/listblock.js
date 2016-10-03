(function( $ ){
    $.fn.listblock = function(options) {
        var defaults = {};
        var settings = $.extend( {}, defaults, options);

        return this.each(function() {
            var $orig = $(this);

            function rawValToItems(x) {
                return x.split(",").map(function(v) {return {email: v};});
            }

            function itemsToRawVal(x) {
                return JSON.stringify(x); 
            }

            function rawToItem(x) {
                if (x) {
                    return {email: x};
                } else {
                    return null;
                }
            }

            function itemToHTML(x) {
                return x.email;
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
                })
                $item.append($del);
                $list.append($item);
                $orig.val(itemsToRawVal(items));
                item._ele = $item;
                items.push(item);
                return $item;
            }

            function removeListElement(item, $item) {
                var idx = items.indexOf(item);
                if(idx>-1) {
                    items.splice(idx, 1);
                }
                $orig.val(itemsToRawVal(items));
                $item.removeClass("show");
                setTimeout(function() {
                    $item.remove();
                }, 500);
            };

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
                addListElement(x, true);
            });
        
            var $input = $("<input>", {type:"text"});
            $input.typeahead({hint: true},
                {name: "contacts", source: function(query, sync) {
                    console.log("hello");
                    sync( ["hello","goodbye"]);
                }}
            );

            $input.on("keypress", function(e) {
                if (e.key=="Enter") {
                    e.preventDefault();
                    $addbutton.trigger("click");
                    console.log("error");
                }
            });

            var $addbutton = $("<button>").append($("<span>", {class: "glyphicon glyphicon-plus-sign"}));
            $addbutton.click(function(e) {
                e.preventDefault();
                var item = rawToItem($input.val());
                if (item) {
                    var existing = items.find(function(x) {return x.email==item.email});
                    if (!existing) {
                        addListElement(item);
                    } else {
                        highlightListElement(existing);
                    }
                }
                $input.val("");
            })
            var $container = $("<div>", {class: "listblock"});
            $container.append($input).append($addbutton).append($list).insertAfter($orig);

            return $orig.hide();
        });
    };
})(jQuery);
