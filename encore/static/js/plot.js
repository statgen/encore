/* eslint-env jquery */
/* global _, d3 */
/* eslint no-unused-vars: ["error", { "vars": "local" }], no-constant-condition: "off"*/

function fmt(format) {
    var args = Array.prototype.slice.call(arguments, 1);
    return format.replace(/{(\d+)}/g, function(match, number) {
        return (typeof args[number] != "undefined") ? args[number] : match;
    });
}

function get_tooltip_text(d) {
    var lines = [];
    if (d) {
        if (d.label) {
            lines.push(_.escape(d.label));
        }
        if (d.chrom && d.pos) {
            lines.push(_.escape(d.chrom) + ":" + _.escape( d.pos.toLocaleString()));
        }
        if (d.ref && d.alt) {
            lines.push(_.escape(d.ref) + " &gt; " + _.escape(d.alt));
        }
        if (d.pval) {
            lines.push("pval: " + _.escape(d.pval));
        } 
        if (d.maf) {
            lines.push("MAF: " + _.escape(d.maf));
        }
    }
    return lines.join("<br/>");
}

function create_gwas_plot(selector, variant_bins, unbinned_variants, chrom_extents, on_variant_click) {

    var get_chrom_offsets = _.memoize(function() {
        var chrom_padding = 2e7;

        var update_chrom_extents = function(variant) {
            if (!(variant.chrom in chrom_extents)) {
                chrom_extents[variant.chrom] = [variant.pos, variant.pos];
            } else if (variant.pos > chrom_extents[variant.chrom][1]) {
                chrom_extents[variant.chrom][1] = variant.pos;
            } else if (variant.pos < chrom_extents[variant.chrom][0]) {
                chrom_extents[variant.chrom][0] = variant.pos;
            }
        };
        if (chrom_extents && Object.keys(chrom_extents).length>0) {
            //precomputed by calling function
        } else {
            variant_bins.forEach(update_chrom_extents);
            unbinned_variants.forEach(update_chrom_extents);
        }

        var chroms = _.sortBy(Object.keys(chrom_extents), Number.parseInt);

        var chrom_genomic_start_positions = {};
        chrom_genomic_start_positions[chroms[0]] = 0;
        for (var i=1; i<chroms.length; i++) {
            chrom_genomic_start_positions[chroms[i]] = chrom_genomic_start_positions[chroms[i-1]] + chrom_extents[chroms[i-1]][1] - chrom_extents[chroms[i-1]][0] + chrom_padding;
        }

        // chrom_offsets are defined to be the numbers that make `get_genomic_position()` work.
        // ie, they leave a gap of `chrom_padding` between the last variant on one chromosome and the first on the next.
        var chrom_offsets = {};
        Object.keys(chrom_genomic_start_positions).forEach(function(chrom) {
            chrom_offsets[chrom] = chrom_genomic_start_positions[chrom] - chrom_extents[chrom][0];
        });

        return {
            chrom_extents: chrom_extents,
            chroms: chroms,
            chrom_genomic_start_positions: chrom_genomic_start_positions,
            chrom_offsets: chrom_offsets
        };
    });

    function get_genomic_position(variant) {
        var chrom_offsets = get_chrom_offsets().chrom_offsets;
        return chrom_offsets[variant.chrom] + variant.pos;
    }

    $(function() {
        var svg_width = $(selector).width();
        var svg_height = 550;
        var plot_margin = {
            "left": 70,
            "right": 30,
            "top": 10,
            "bottom": 50
        };
        var plot_width = svg_width - plot_margin.left - plot_margin.right;
        var plot_height = svg_height - plot_margin.top - plot_margin.bottom;

        var gwas_svg = d3.select(selector).append("svg")
            .attr("id", "gwas_svg")
            .attr("width", svg_width)
            .attr("height", svg_height)
            .style("display", "block")
            .style("margin", "auto");
        var gwas_plot = gwas_svg.append("g")
            .attr("id", "gwas_plot")
            .attr("transform", fmt("translate({0},{1})", plot_margin.left, plot_margin.top));

        // Significance Threshold line
        var significance_threshold = 5e-8;
        var significance_threshold_tooltip = d3.tip()
            .attr("class", "d3-tip")
            .html("Significance Threshold: 5E-8")
            .offset([-8,0]);
        gwas_svg.call(significance_threshold_tooltip);

        var genomic_position_extent = (function() {
            let chrom_offsets = get_chrom_offsets()
            return d3.extent(
                chrom_offsets.chroms.flatMap(x => chrom_extents[x].map(y =>
                    get_genomic_position({chrom: x, pos: y})))
            );
        })();

        var x_scale = d3.scale.linear()
            .domain(genomic_position_extent)
            .range([0, plot_width]);

        var max_neglog10_pval = (function() {
            if (unbinned_variants.length > 0) {
                return d3.max(unbinned_variants, function(d) {
                    return -Math.log10(d.pval);
                });
            }
            return d3.max(variant_bins, function(bin) {
                var opts = [.01];
                if (bin.neglog10_pval_extents && bin.neglog10_pval_extents.length) {
                    opts.push(d3.max(_.flatten(bin.neglog10_pval_extents)));
                }
                if (bin.neglog10_pvals && bin.neglog10_pvals.length) {
                    opts.push(d3.max(bin.neglog10_pvals));
                }
                return d3.max(opts);
            });
        })();

        var y_scale = d3.scale.linear()
            .domain([Math.max(10, max_neglog10_pval), 0])
            .range([0, plot_height]);

        var color_by_chrom = d3.scale.ordinal()
            .domain(get_chrom_offsets().chroms)
            .range(["rgb(120,120,186)", "rgb(0,0,66)"]);
        //colors to maybe sample from later:
        //.range(['rgb(120,120,186)', 'rgb(0,0,66)', 'rgb(44,150,220)', 'rgb(40,60,80)', 'rgb(33,127,188)', 'rgb(143,76,176)']);

        gwas_plot.append("line")
            .attr("x1", 0)
            .attr("x2", plot_width)
            .attr("y1", y_scale(-Math.log10(significance_threshold)))
            .attr("y2", y_scale(-Math.log10(significance_threshold)))
            .attr("stroke-width", "5px")
            .attr("stroke", "lightgray")
            .attr("stroke-dasharray", "10,10")
            .on("mouseover", significance_threshold_tooltip.show)
            .on("mouseout", significance_threshold_tooltip.hide);

        // Points & labels
        var point_tooltip = d3.tip()
            .attr("class", "d3-tip")
            .html(function(d) {
                return get_tooltip_text(d);
            })
            .style("background", "rgba(0, 0, 0, 0.7)")
            .style("color", "#FFFFFF")
            .style("padding", "5px")
            .style("border", "1px solid #000000")
            .style("border-radius", "3px")
            .offset([-6,0]);
        gwas_svg.call(point_tooltip);

        function pp1() {
            gwas_plot.append("g")
                .attr("class", "variant_hover_rings")
                .selectAll("a.variant_hover_ring")
                .data(unbinned_variants)
                .enter()
                .append("a")
                .attr("class", "variant_hover_ring")
                // .attr('xlink:href', function(d) {
                //     return fmt('/variant/{0}-{1}-{2}-{3}', d.chrom, d.pos, d.ref, d.alt);
                // })
                .append("circle")
                .attr("cx", function(d) {
                    return x_scale(get_genomic_position(d));
                })
                .attr("cy", function(d) {
                    return y_scale(-Math.log10(d.pval));
                })
                .attr("r", 7)
                .style("opacity", 0)
                .style("stroke-width", 1)
                .on("mouseover", function(d) {
                    //Note: once a tooltip has been explicitly placed once, it must be explicitly placed forever after.
                    var target_node = document.getElementById(fmt("variant-point-{0}-{1}-{2}-{3}", d.chrom, d.pos, d.ref, d.alt));
                    point_tooltip.show(d, target_node);
                })
                .on("mouseout", point_tooltip.hide)
                .on("click", function(d) { on_variant_click(d.chrom, d.pos, d.ref, d.alt); });
        }
        pp1();

        function pp2() {
            gwas_plot.append("g")
                .attr("class", "variant_points")
                .selectAll("a.variant_point")
                .data(unbinned_variants)
                .enter()
                .append("a")
                .attr("class", "variant_point")
                // .attr('xlink:href', function(d) {
                //     return fmt('/variant/{0}-{1}-{2}-{3}', d.chrom, d.pos, d.ref, d.alt);
                // })
                .append("circle")
                .attr("id", function(d) {
                    return fmt("variant-point-{0}-{1}-{2}-{3}", d.chrom, d.pos, d.ref, d.alt);
                })
                .attr("cx", function(d) {
                    return x_scale(get_genomic_position(d));
                })
                .attr("cy", function(d) {
                    return y_scale(-Math.log10(d.pval));
                })
                .attr("r", 2.3)
                .style("fill", function(d) {
                    return color_by_chrom(d.chrom);
                })
                .on("mouseover", function(d) {
                    //Note: once a tooltip has been explicitly placed once, it must be explicitly placed forever after.
                    point_tooltip.show(d, this);
                })
                .on("mouseout", point_tooltip.hide)
                .on("click", function(d) { on_variant_click(d.chrom, d.pos, d.ref, d.alt); });
        }
        pp2();

        function pp3() { // drawing the ~60k binned variant circles takes ~500ms.  The (far fewer) unbinned variants take much less time.
            var bins = gwas_plot.append("g")
                .attr("class", "bins")
                .selectAll("g.bin")
                .data(variant_bins)
                .enter()
                .append("g")
                .attr("class", "bin")
                .each(function(d) { //todo: do this in a forEach
                    d.x = x_scale(get_genomic_position(d));
                    d.color = color_by_chrom(d.chrom);
                });
            bins.selectAll("circle.binned_variant_point")
                .data(_.property("neglog10_pvals"))
                .enter()
                .append("circle")
                .attr("class", "binned_variant_point")
                .attr("cx", function(d, i, parent_i) {
                    return variant_bins[parent_i].x;
                })
                .attr("cy", function(neglog10_pval) {
                    return y_scale(neglog10_pval);
                })
                .attr("r", 2.3)
                .style("fill", function(d, i, parent_i) {
                    // return color_by_chrom(d3.select(this.parentNode).datum().chrom); //slow
                    // return color_by_chrom(this.parentNode.__data__.chrom); //slow?
                    // return this.parentNode.__data__.color;
                    return variant_bins[parent_i].color;
                });
            bins.selectAll("circle.binned_variant_line")
                .data(_.property("neglog10_pval_extents"))
                .enter()
                .append("line")
                .attr("class", "binned_variant_line")
                .attr("x1", function(d, i, parent_i) { return variant_bins[parent_i].x; })
                .attr("x2", function(d, i, parent_i) { return variant_bins[parent_i].x; })
                .attr("y1", function(d) { return y_scale(d[0]); })
                .attr("y2", function(d) { return y_scale(d[1]); })
                .style("stroke", function(d, i, parent_i) { return variant_bins[parent_i].color; })
                .style("stroke-width", 4.6)
                .style("stroke-linecap", "round");
        }
        pp3();

        // Axes
        var yAxis = d3.svg.axis()
            .scale(y_scale)
            .orient("left")
            .tickFormat(d3.format("d"));
        gwas_plot.append("g")
            .attr("class", "y axis")
            .attr("transform", "translate(-8,0)") // avoid letting points spill through the y axis.
            .call(yAxis);

        gwas_svg.append("text")
            .style("text-anchor", "middle")
            .attr("transform", fmt("translate({0},{1})rotate(-90)",
                plot_margin.left*.4,
                plot_height/2 + plot_margin.top))
            .text("-log\u2081\u2080(pvalue)");

        var chroms_and_midpoints = (function() {
            var v = get_chrom_offsets();
            return v.chroms.map(function(chrom) {
                return {
                    chrom: chrom,
                    midpoint: v.chrom_genomic_start_positions[chrom] + (v.chrom_extents[chrom][1] - v.chrom_extents[chrom][0]) / 2
                };
            });
        })();

        gwas_svg.selectAll("text.chrom_label")
            .data(chroms_and_midpoints)
            .enter()
            .append("text")
            .style("text-anchor", "middle")
            .attr("transform", function(d) {
                return fmt("translate({0},{1})",
                    plot_margin.left + x_scale(d.midpoint),
                    plot_height + plot_margin.top + 20);
            })
            .text(function(d) {
                return d.chrom.replace("chr","");
            })
            .style("fill", function(d) {
                return color_by_chrom(d.chrom);
            });
    });
}


function create_qq_plot(selector, qq_plot_data, qq_plot_meta, on_variant_click) {
    var layers = qq_plot_data.layers;
    layers.forEach(function(layer, i) {
        if (!layer.color) {
            layer.color = ["#e66101", "#fdb863", "#b2abd2", "#5e3c99"][i];
        }
    });
    $(function() {
        var firstdata = function(layer) {if (layer.unbinned_variants && layer.unbinned_variants.length) {return layer.unbinned_variants;} else {return layer.variant_bins;}};
        var exp_max = d3.max(layers, function(layer) { return d3.max(firstdata(layer), _.property(0)); });
        var obs_max = d3.max(layers, function(layer) { return d3.max(firstdata(layer), _.property(1)); });
        // Constrain obs_max in [exp_max, 9.01]. `9.01` preserves the tick `9`.
        //obs_max = Math.max(exp_max, Math.min(9.01, obs_max));

        var svg_width = 600; //$(selector).width();
        var plot_margin = {
            "left": 70,
            "right": 30,
            "top": 10,
            "bottom": 120
        };
        var plot_width = svg_width - plot_margin.left - plot_margin.right;
        // Size the plot to make things square.  This way, x_scale and y_scale should be exactly equivalent.
        var plot_height = plot_width / exp_max * obs_max;
        if ( plot_height>600 ) {
            plot_height = 600;
        }
        var svg_height = plot_height + plot_margin.top + plot_margin.bottom;

        var qq_container = d3.select(selector).append("div")
            .style("text-align", "center")
            .attr("height", svg_height)
            .style("vertical-align", "middle");
        var qq_svg =  qq_container.append("svg")
            .attr("id", "qq_svg")
            .attr("width", svg_width)
            .attr("height", svg_height)
            .style("display", "inline-block");
        var qq_plot = qq_svg.append("g")
            .attr("id", "qq_plot")
            .attr("transform", fmt("translate({0},{1})", plot_margin.left, plot_margin.top));

        var x_scale = d3.scale.linear()
            .domain([0, exp_max])
            .range([0, plot_width]);
        var y_scale = d3.scale.linear()
            .domain([0, obs_max])
            .range([plot_height, 0]);

        // "trumpet" CI path
        // grab confidence intervals for largest set
        var qq_ci_trumpet_points = layers.reduce(function(a,b) {
            if (b.count>a.count && b.conf_int) {
                return {count: b.count, points: b.conf_int};
            } else {
                return a;
            }
        }, {count:0, points:null}).points;

        if (qq_ci_trumpet_points ) {
            var area = d3.svg.area()
                .x( function(d) {
                    return x_scale(d[0]);
                }).y0( function(d) {
                    return y_scale(d[1] + .05);
                }).y1( function(d) {
                    return y_scale(d[2] - .05);
                });
            qq_plot.append("path")
                .attr("class", "trumpet_ci")
                .datum(qq_ci_trumpet_points)
                .attr("d", area)
                .style("fill", "lightgray");
        }

        // points
        var qqpoints = qq_plot.append("g")
            .selectAll("g.qq_points")
            .data(layers)
            .enter()
            .append("g")
            .attr("class", "qq_points");
        qqpoints
            .selectAll("circle.qq_bins")
            .data(_.property("variant_bins"))
            .enter()
            .append("circle")
            .attr("cx", function(d) { return x_scale(d[0]); })
            .attr("cy", function(d) { return y_scale(d[1]); })
            .attr("r", 1.5)
            .attr("fill", function (d, i, parent_index) {
                return layers[parent_index].color;
            });
        var point_tooltip = d3.tip()
            .attr("class", "d3-tip")
            .html(function(d) {
                return get_tooltip_text(d[2]);
            })
            .style("background", "rgba(0, 0, 0, 0.7)")
            .style("color", "#FFFFFF")
            .style("padding", "5px")
            .style("border", "1px solid #000000")
            .style("border-radius", "3px")
            .offset([-6,0]);
        qq_plot.call(point_tooltip);
        qqpoints = qqpoints
            .selectAll("circle.qq_point")
            .data(_.property("unbinned_variants"))
            .enter()
            .append("circle")
            .attr("cx", function(d) { return x_scale(d[0]); })
            .attr("cy", function(d) { return y_scale(d[1]); })
            .attr("r", 3)
            .attr("fill", function (d, i, parent_index) {
                return layers[parent_index].color;
            }).
            on("mouseover", function(d) {point_tooltip.show(d, this);}).
            on("mouseout", point_tooltip.hide);
        if (on_variant_click) {
            qqpoints.
            style("cursor", "pointer").
            on("click", function(d) {
                var dat = d[2];
                on_variant_click(dat.CHROM, dat.BEGIN);
            });
        }

        if (layers.length > 1) {
            if (false) {
                // SVG Legend
                qq_svg.append("g")
                .attr("transform", fmt("translate({0},{1})",
                    plot_margin.left + plot_width,
                    plot_margin.top + plot_height + 70))
                .selectAll("text.legend-items")
                .data(layers)
                .enter()
                .append("text")
                .attr("text-anchor", "end")
                .attr("y", function(d,i) {
                    return i + "em";
                })
                .text(function(d) {
                    return fmt("{0} \u2264 MAF < {1} ({2})",
                        d.maf_range[0].toFixed(3),
                        d.maf_range[1].toFixed(3),
                        d.count);
                })
                .style("font-size","14px")
                .attr("fill", function(d) {
                    return d.color;
                });
            } else {
                //HTML Legend
                var tbl = qq_container.append("table")
                    .style("width","auto")
                    .style("vertical-align", "top")
                    .style("display","inline-block");
                var tr = tbl.selectAll("tr")
                    .data(layers).enter()
                    .append("tr");
                tr.append("td").append(function(d) {
                    return d3.select(document.createElement("div"))   
                        .style("width", "20px")
                        .style("height", "20px")
                        .style("background",  d.color)
                        .node();
                });
                tr.append("td").text(function(d) {
                    if (d.gc && d.gc["50"]) {
                        return fmt("{0} \u2264 MAF < {1} (N={2}, \u03BB={3})",
                            d.maf_range[0].toFixed(3),
                            d.maf_range[1].toFixed(3),
                            d.count,
                            d.gc["50"].toFixed(3));
                    } else {
                        return fmt("{0} \u2264 MAF < {1} ({2})",
                            d.maf_range[0].toFixed(3),
                            d.maf_range[1].toFixed(3),
                            d.count);
                    }
                });
            }
        } else if (layers.length == 1) {
            var p = qq_container.append("p")
                .style("width","auto")
                .style("vertical-align", "top")
                .style("display","inline-block");
            var d = layers[0];
            var txt; 
            if (d.gc && d.gc["50"]) {
                txt = fmt("N={0}, \u03BB={1}",
                    d.count,
                    d.gc["50"].toFixed(3));
            } else {
                txt = fmt("N={0}",
                    d.count);
            }
            p.text(txt);
        }

        // Axes
        var xAxis = d3.svg.axis()
            .scale(x_scale)
            .orient("bottom")
            .innerTickSize(-plot_height) // this approach to a grid is taken from <http://bl.ocks.org/hunzy/11110940>
            .outerTickSize(0)
            .tickPadding(7)
            .tickFormat(d3.format("d")) //integers
            .tickValues(_.range(exp_max)); //prevent unlabeled, non-integer ticks.
        qq_plot.append("g")
            .attr("class", "x axis")
            .attr("transform", fmt("translate(0,{0})", plot_height))
            .call(xAxis);

        var yAxis = d3.svg.axis()
            .scale(y_scale)
            .orient("left")
            .innerTickSize(-plot_width)
            .outerTickSize(0)
            .tickPadding(7)
            .tickFormat(d3.format("d")); //integers
            //.tickValues(_.range(obs_max)); //prevent unlabeled, non-integer ticks.
        qq_plot.append("g")
            .attr("class", "y axis")
            .call(yAxis);

        qq_svg.append("text")
            .style("text-anchor", "middle")
            .attr("transform", fmt("translate({0},{1})rotate(-90)",
                plot_margin.left*.4,
                plot_margin.top + plot_height/2))
            .text("observed -log\u2081\u2080(p)");

        qq_svg.append("text")
            .style("text-anchor", "middle")
            .attr("transform", fmt("translate({0},{1})",
                plot_margin.left + plot_width/2,
                plot_margin.top + plot_height + 40))
            .text("expected -log\u2081\u2080(p)");
    });
}
