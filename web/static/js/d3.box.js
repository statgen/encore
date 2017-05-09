/* eslint-env jquery */
/* global d3 */

(function() {
// from http://bl.ocks.org/jensgrubert/7789216
// Inspired by http://informationandvisualization.de/blog/box-plot
    d3.box = function() {
        var width = 1,
            height = 1,
            duration = 0,
            domain = null,
            value = Number,
            whiskers = boxWhiskers,
            quartiles = boxQuartiles,
            outliers = boxOutliers,
            boxstats = boxStats,
            showLabels = true, // whether or not to show text labels
            tickFormat = null;

      // For each small multiple…
        function box(g) {
            g.each(function(data, i) {

                var g = d3.select(this);
                var stats = boxstats(data, i, value, whiskers, quartiles, outliers);
                // Compute the new x-scale.
                var x1 = d3.scale.linear()
                  .domain(domain && domain.call(this, data, i) || [stats.min, stats.max])
                  .range([height, 0]);

                // Retrieve the old x-scale, if this is an update.
                var x0 = this.__chart__ || d3.scale.linear()
                  .domain([0, Infinity])
                  .range(x1.range());

                // Stash the new scale.
                this.__chart__ = x1;

      // Note: the box, median, and box tick elements are fixed in number,
      // so we only have to handle enter and update. In contrast, the outliers
      // and other elements are variable, so we need to exit them! Variable
      // elements also fade in and out.

                g.style("pointer-events", "all");
                //rect to capture mouse events
                g.append("rect")
                  .style("visibility", "hidden")
                  .attr("x", 0)
                  .attr("y", 0)
                  .attr("width", width)
                  .attr("height", height);
                var tooltip = (function(x1) {
                    var valLabel = g.append("text")
                      .style("display", "none")
                      .attr("class", "qtip")
                      .attr("dx", 6)
                      .attr("x", width)
                      .attr("y", 0)
                      .attr("alignment-baseline", "central")
                      .attr("text-anchor", "start");
                    function show(at) {
                        valLabel
                          .text(d3.format(".2f")(at))
                          .attr("y", x1(at))
                          .style("display", "block");
                    }
                    function hide() {
                        valLabel.style("display", "none");
                    }
                    return {
                        show: show,
                        hide: hide
                    };
                })(x1);
                g.on("mousemove", function() {
                    var mouse_y = d3.mouse(this)[1];
                    var labelable = stats.quartiles.concat(stats.whiskers);
                    var q_y = labelable.map(x1);
                    var best_match = 0;
                    var min_dist = Math.abs(q_y[0]-mouse_y);
                    for(var i=1; i<q_y.length; i++) {
                        var dist = Math.abs(q_y[i]-mouse_y);
                        if (dist < min_dist) {
                            min_dist = dist;
                            best_match = i;
                        }
                    }
                    if (min_dist<8) {
                        tooltip.show(labelable[best_match]);
                    } else {
                        tooltip.hide();
                    }
                }).on("mouseout", function() {tooltip.hide();});
                // Update center line: the vertical line spanning the whiskers.
                var center = g.selectAll("line.center")
                  .data(stats.whiskers ? [stats.whiskers] : []);

                 //vertical line
                center.enter().insert("line", "rect")
                  .attr("class", "center")
                  .attr("x1", width / 2)
                  .attr("y1", function(d) { return x0(d[0]); })
                  .attr("x2", width / 2)
                  .attr("y2", function(d) { return x0(d[1]); })
                  .style("opacity", 1e-6)
                .transition()
                  .duration(duration)
                  .style("opacity", 1)
                  .attr("y1", function(d) { return x1(d[0]); })
                  .attr("y2", function(d) { return x1(d[1]); });

                center.transition()
                  .duration(duration)
                  .style("opacity", 1)
                  .attr("y1", function(d) { return x1(d[0]); })
                  .attr("y2", function(d) { return x1(d[1]); });

                center.exit().transition()
                  .duration(duration)
                  .style("opacity", 1e-6)
                  .attr("y1", function(d) { return x1(d[0]); })
                  .attr("y2", function(d) { return x1(d[1]); })
                  .remove();

                // Update innerquartile box.
                var box = g.selectAll("rect.box")
                  .data([stats.quartiles]);

                box.enter().append("rect")
                  .attr("class", "box")
                  .attr("x", 0)
                  .attr("y", function(d) { return x0(d[2]); })
                  .attr("width", width)
                  .attr("height", function(d) { return x0(d[0]) - x0(d[2]); })
                .transition()
                  .duration(duration)
                  .attr("y", function(d) { return x1(d[2]); })
                  .attr("height", function(d) { return x1(d[0]) - x1(d[2]); });

                box.transition()
                  .duration(duration)
                  .attr("y", function(d) { return x1(d[2]); })
                  .attr("height", function(d) { return x1(d[0]) - x1(d[2]); });


                // Update median line.
                var medianLine = g.selectAll("line.median")
                  .data([stats.quartiles[1]]);

                medianLine.enter().append("line")
                  .attr("class", "median")
                  .attr("x1", 0)
                  .attr("y1", x0)
                  .attr("x2", width)
                  .attr("y2", x0)
                .transition()
                  .duration(duration)
                  .attr("y1", x1)
                  .attr("y2", x1);

                medianLine.transition()
                  .duration(duration)
                  .attr("y1", x1)
                  .attr("y2", x1);

                // Update whiskers.
                var whisker = g.selectAll("line.whisker")
                  .data(stats.whiskers || []);

                whisker.enter().insert("line", "circle, text")
                  .attr("class", "whisker")
                  .attr("x1", 0)
                  .attr("y1", x0)
                  .attr("x2", 0 + width)
                  .attr("y2", x0)
                  .style("opacity", 1e-6)
                .transition()
                  .duration(duration)
                  .attr("y1", x1)
                  .attr("y2", x1)
                  .style("opacity", 1);

                whisker.transition()
                  .duration(duration)
                  .attr("y1", x1)
                  .attr("y2", x1)
                  .style("opacity", 1);

                whisker.exit().transition()
                  .duration(duration)
                  .attr("y1", x1)
                  .attr("y2", x1)
                  .style("opacity", 1e-6)
                  .remove();

                // Update outliers.
                var outlier = g.selectAll("circle.outlier")
                  .data(stats.outliers, Number);

                outlier.enter().insert("circle", "text")
                  .attr("class", "outlier")
                  .attr("r", 2)
                  .attr("cx", width / 2)
                  .attr("cy", function(x) { return x0(x); })
                  .style("opacity", 1e-6)
                .transition()
                  .duration(duration)
                  .attr("cy", function(x) { return x1(x); })
                  .style("opacity", 1);

                outlier.transition()
                  .duration(duration)
                  .attr("cy", function(x) { return x1(x); })
                  .style("opacity", 1);

                outlier.exit().transition()
                  .duration(duration)
                  .attr("cy", function(x) { return x1(x); })
                  .style("opacity", 1e-6)
                  .remove();

              // Compute the tick format.
                var format = tickFormat || x1.tickFormat(8);

              // Update box ticks.
                var boxTick = g.selectAll("text.box")
                  .data(stats.quartiles);
                if(showLabels == true) {
                    boxTick.enter().append("text")
                      .attr("class", "box")
                      .attr("dy", ".3em")
                      .attr("dx", function(d, i) { return i & 1 ? 6 : -6; })
                      .attr("x", function(d, i) { return i & 1 ?  + width : 0; })
                      .attr("y", x0)
                      .attr("text-anchor", function(d, i) { return i & 1 ? "start" : "end"; })
                      .text(format)
                    .transition()
                      .duration(duration)
                      .attr("y", x1);
                }
         
                boxTick.transition()
                  .duration(duration)
                  .text(format)
                  .attr("y", x1);

          // Update whisker ticks. These are handled separately from the box
          // ticks because they may or may not exist, and we want don't want
          // to join box ticks pre-transition with whisker ticks post-.
                var whiskerTick = g.selectAll("text.whisker")
                  .data(stats.whiskers || []);
                if(showLabels == true) {
                    whiskerTick.enter().append("text")
                      .attr("class", "whisker")
                      .attr("dy", ".3em")
                      .attr("dx", 6)
                      .attr("x", width)
                      .attr("y", x0)
                      .text(format)
                      .style("opacity", 1e-6)
                    .transition()
                      .duration(duration)
                      .attr("y", x1)
                      .style("opacity", 1);
                }
                whiskerTick.transition()
                  .duration(duration)
                  .text(format)
                  .attr("y", x1)
                  .style("opacity", 1);

                whiskerTick.exit().transition()
                  .duration(duration)
                  .attr("y", x1)
                  .style("opacity", 1e-6)
                  .remove();
            });
            d3.timer.flush();
        }

        box.width = function(x) {
            if (!arguments.length) return width;
            width = x;
            return box;
        };

        box.height = function(x) {
            if (!arguments.length) return height;
            height = x;
            return box;
        };

        box.tickFormat = function(x) {
            if (!arguments.length) return tickFormat;
            tickFormat = x;
            return box;
        };

        box.duration = function(x) {
            if (!arguments.length) return duration;
            duration = x;
            return box;
        };

        box.domain = function(x) {
            if (!arguments.length) return domain;
            domain = x == null ? x : d3.functor(x);
            return box;
        };

        box.value = function(x) {
            if (!arguments.length) return value;
            value = x;
            return box;
        };

        box.whiskers = function(x) {
            if (!arguments.length) return whiskers;
            whiskers = x;
            return box;
        };
  
        box.showLabels = function(x) {
            if (!arguments.length) return showLabels;
            showLabels = x;
            return box;
        };

        box.quartiles = function(x) {
            if (!arguments.length) return quartiles;
            quartiles = x;
            return box;
        };

        box.boxstats = function(x) {
            if (!arguments.length) return boxstats;
            boxstats = x;
            return box;
        };

        return box;
    };

    function boxStats(data, i, value, whiskers, quartiles, outliers) {
        var d = value(data).sort(d3.ascending);

        var n = d.length,
            min = d[0],
            max = d[n - 1];

        // Compute quartiles. Must return exactly 3 elements.
        var quartileData = d.quartiles = quartiles(d);

        // Compute whiskers. Must return exactly 2 elements, or null.
        var whiskerIndices = whiskers && whiskers.call(this, d, i),
            whiskerData = whiskerIndices && whiskerIndices.map(function(i) { return d[i]; });

        // Compute outliers. If no whiskers are specified, all data are "outliers".
       // We compute the outliers as indices, so that we can join across transitions!
        var outlierIndices = whiskerIndices
          ? d3.range(0, whiskerIndices[0]).concat(d3.range(whiskerIndices[1] + 1, n))
          : d3.range(n);

        var outlierData = outliers 
            ? outliers(d)
            : outlierIndices.map(function(i) {return d[i];});

        return {
            min: min,
            max: max,
            n: n,
            whiskers: whiskerData,
            outliers: outlierData,
            quartiles: quartileData
        };
    }

    function boxWhiskers(d) {
        return [0, d.length - 1];
    }

    function boxQuartiles(d) {
        return [
            d3.quantile(d, .25),
            d3.quantile(d, .5),
            d3.quantile(d, .75)
        ];
    }

    function boxOutliers(d) {
        return [
            d3.quantile(d, .25),
            d3.quantile(d, .5),
            d3.quantile(d, .75)
        ];
    }
})();
