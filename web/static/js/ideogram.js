/* global d3 */
/* eslint no-console: "off" */

var Ideogram = function(selector) {
    this.selector = selector;
    if (!selector) {
        throw("Must supply a selector to initialize ideogram");
    }
    this.svg = d3.select(selector).append("svg")
        .attr("width",800)
        .attr("height", 450);
    this.svg.append("g")
        .attr("class", "chromosome");
    this.svg.append("g")
        .attr("class", "outline");
    if (this.svg.empty()) {
        throw("Could not find '" + selector + "', unable to initialize ideogram");
    }

    if (d3.tip) {
        this.tooltip = d3.tip()
            .attr("class", "d3-tip")
            .offset([-10,0])
            .html(function(d) {
                return d.tooltip;
            });
        this.svg.call(this.tooltip);
    }

    this.regions = [];
};

Ideogram.prototype.draw = function(build) {
    build = build || "GRCh37";
    var layout = this.getLayout(this.layouts[build]);
    var positions = this.calculatePositions(layout);
    this.drawOutlines(positions);
    this.drawRegions(positions, this.regions);
    this.drawLabels(positions);
};

Ideogram.prototype.setRegions = function(regions) {
    this.regions = regions;
};

Ideogram.prototype.toPointsString = function(d) {
    return d.points.map(function(d) {return [d[0].toFixed(2), d[1].toFixed(2)].join(",");}).join(" ");
};

Ideogram.prototype.drawLabels = function(positions, opts) {
    opts = opts || {};
    var labels = [];
    positions.positions.forEach(function(ele) {
        if (ele.type && ele.type=="label") {
            labels.push({
                text: ele.text,
                x: (ele.x0 + ele.x1)/2,
                y: (ele.y0 + ele.y1)/2
            });
        }
    }.bind(this));
    this.svg.select("g.outline").selectAll("text.label")
        .data(labels)
        .enter().append("text")
        .attr("x", function(d) {return d.x;})
        .attr("y", function(d) {return d.y+5;})
        .attr("text-anchor", "middle")
        .text(function(d) {return d.text;});
};

Ideogram.prototype.drawOutlines = function(positions, opts) {
    opts = opts || {fill: "none", stroke: "black", "stroke-width": 1.5};
    var regions = []; 
    positions.positions.forEach(function(ele) {
        if (ele.type && ele.type=="chr") {
            regions.push({points: this.getRegionPath(positions, ele.name)});
        }
    }.bind(this));
    this.svg.select("g.outline").selectAll("polygon.chroutline")
        .data(regions)
        .enter().append("polygon")
        .attr("class"," chroutline")
        .attr("points", this.toPointsString)
        .attr(opts);
};

Ideogram.prototype.drawRegions = function(positions, regions, opts) {
    opts = opts || {fill: "#c8c8c8"};
    var polys = []; 
    var hasToolTips = false;
    for(var i=0; i<regions.length; i++) {
        var pts = this.getRegionPath(positions,
            regions[i].chrom, 
            regions[i].start,
            regions[i].stop);
        if (pts) {
            polys.push({points: pts, 
                fill: regions[i].fill || opts.fill,
                tooltip: regions[i].tooltip});
            hasToolTips |= !!(regions[i].tooltip);
        }
    }
    var r = this.svg.select("g.chromosome").selectAll("polygon.region")
        .data(polys);
    r.enter().append("polygon")
        .attr("class", "region");
    r.exit().remove();
    r.attr("points", this.toPointsString);
    r.attr("fill", function(d) {return d.fill || opts.fill;});
    if (hasToolTips) {
        if (this.tooltip) {
            r.on("mouseover", function(d) {
                if (d.tooltip) {
                    this.tooltip.show(d);
                }
            }.bind(this));
            r.on("mouseout", this.tooltip.hide);
        } else {
            console.warning("Tooltips not available (include d3.tip)");
        }
    }
};

Ideogram.prototype.layouts = {};
Ideogram.prototype.layouts["GRCh37"] = [
    {chr: "chr1", center: 125000000, end: 249250621},
    {chr: "chr2", center: 93300000, end: 243199373},
    {chr: "chr3", center: 91000000, end: 198022430},
    {chr: "chr4", center: 50400000, end: 191154276},
    {chr: "chr5", center: 48400000, end: 180915260},
    {chr: "chr6", center: 61000000, end: 171115067},
    {chr: "chr7", center: 59900000, end: 159138663},
    {chr: "chr8", center: 45600000, end: 146364022},
    {chr: "chr9", center: 49000000, end: 141213431},
    {chr: "chr10", center: 40200000, end: 135534747},
    {chr: "chr11", center: 53700000, end: 135006516},
    {chr: "chr12", center: 35800000, end: 133851895},
    {chr: "chr13", center: 17900000, end: 115169878},
    {chr: "chr14", center: 17600000, end: 107349540},
    {chr: "chr15", center: 19000000, end: 102531392},
    {chr: "chr16", center: 36600000, end: 90354753},
    {chr: "chr17", center: 24000000, end: 81195210},
    {chr: "chr18", center: 17200000, end: 78077248},
    {chr: "chr19", center: 26500000, end: 59128983},
    {chr: "chr20", center: 27500000, end: 63025520},
    {chr: "chr21", center: 13200000, end: 48129895},
    {chr: "chr22", center: 14700000, end: 51304566},
    {chr: "chrX", center: 60600000, end: 155270560},
    {chr: "chrY", center: 12500000, end: 59373566}
];

Ideogram.prototype.layouts["GRCh38"] = [
    {chr: "chr1", center: 123400000, end: 248956422},
    {chr: "chr2", center: 93900000, end: 242193529},
    {chr: "chr3", center: 90900000, end: 198295559},
    {chr: "chr4", center: 50000000, end: 190214555},
    {chr: "chr5", center: 48800000, end: 181538259},
    {chr: "chr6", center: 59800000, end: 170805979},
    {chr: "chr7", center: 60100000, end: 159345973},
    {chr: "chr8", center: 45200000, end: 145138636},
    {chr: "chr9", center: 43000000, end: 138394717},
    {chr: "chr10", center: 39800000, end: 133797422},
    {chr: "chr11", center: 53400000, end: 135086622},
    {chr: "chr12", center: 35500000, end: 133275309},
    {chr: "chr13", center: 17700000, end: 114364328},
    {chr: "chr14", center: 17200000, end: 107043718},
    {chr: "chr15", center: 19000000, end: 101991189},
    {chr: "chr16", center: 36800000, end: 90338345},
    {chr: "chr17", center: 25100000, end: 83257441},
    {chr: "chr18", center: 18500000, end: 80373285},
    {chr: "chr19", center: 26200000, end: 58617616},
    {chr: "chr20", center: 28100000, end: 64444167},
    {chr: "chr21", center: 12000000, end: 46709983},
    {chr: "chr22", center: 15000000, end: 50818468},
    {chr: "chrX", center: 61000000, end: 156040895},
    {chr: "chrY", center: 10400000, end: 57227415}
];


Ideogram.prototype.getLayout = function(chrs, opts) {
    opts = opts || {};
    var rows = [];
    var maxextent = 0;
    var lookup = {};
    var drawX = true;
    var add2col = function(i, a,b) {
        var cols = [];
        var rowextent = a.end + b.end;
        if (rowextent > maxextent) {maxextent = rowextent;}
        cols.push({type: "label", width: "45px", text: a.chr});
        cols.push({type: "chr", name: a.chr, center: a.center, end: a.end});
        cols.push({type: "gap", min_width: "25px"});
        cols.push({type: "chr", name: b.chr, center: b.center, end: b.end});
        cols.push({type: "label", width: "45px", text: b.chr});
        rows.push({cols: cols});
        lookup[a.chr] = [i, 1];
        lookup[b.chr] = [i, 3];
    };
    var add1col = function(i, a) {
        var cols = [];
        var rowextent = a.end;
        if (rowextent > maxextent) {maxextent = rowextent;}
        cols.push({type: "gap", min_width: "25px"});
        cols.push({type: "label", width: "45px", text: a.chr});
        cols.push({type: "chr", name: a.chr, center: a.center, end: a.end});
        cols.push({type: "gap", min_width: "25px"});
        rows.push({cols: cols});
        lookup[a.chr] = [i, 2];
    };
    for(var i=0; i<11; i++) {
        var a = chrs[i];
        var b = chrs[21-i];
        add2col(i,a,b);
    }
    if (drawX) {
        add1col(i, chrs[22]);
    }
    return {rows: rows, max_row_extent: maxextent,
        corner_ease: 5, chr: lookup};
};

Ideogram.prototype.getRowWidths = function(row, width, scale) {
    var fixed = 0;
    var gaps = [];
    var data = 0;
    var widths = row.cols.map(function() {return 0;});
    var remainWidth = width;
    row.cols.map(function(col, idx) {
        if (col.width) {
            widths[idx] = parseFloat(col.width);
            fixed += widths[idx];
            remainWidth -= widths[idx];
        } else if (col.min_width) {
            fixed = fixed + parseFloat(col.min_width);
        } else if (col.end) {
            data = data + col.end;
        }
        if (col.type && col.type=="gap") {
            gaps.push(idx);
        }
    });
    scale = (width - fixed)/scale;
    row.cols.map(function(col, idx) {
        if (col.type =="chr") {
            var w = Math.round(col.end * scale);
            widths[idx] = w;
            remainWidth -= w;
        }
    });
    if (gaps.length > 0 && remainWidth>0) {
        gaps.map(function(g) {
            widths[g] = remainWidth/gaps.length; 
        });
    }
    return widths;
};

Ideogram.prototype.getContour = function(start, stop, landmarks, ease) {
    var contour =[];
    var xp = [landmarks[0], landmarks[0] + ease,
        landmarks[1]-ease, landmarks[1], landmarks[1]+ease,
        landmarks[2]-ease, landmarks[2]];
    var yp = [ease, 0, 0, ease, 0, 0, ease];
    if (start < xp[0]) {start = xp[0];}
    if (stop > xp[xp.length-1]) {stop = xp[xp.length-1];}
    var interp = function(val, i) {
        return [val-xp[0], (val-xp[i])/(xp[i+1]-xp[i]) * (yp[i+1]-yp[i]) + yp[i]];
    };
    for(var i=0; i < xp.length-1; i++) {
        if (start >= xp[i] && start < xp[i+1]) {
            contour.push(interp(start, i));
        }
        if (stop > xp[i] && stop <= xp[i+1]) {
            contour.push(interp(stop, i));
        }
        if (start < xp[i+1] && stop > xp[i+1]) {
            contour.push([xp[i+1]-xp[0], yp[i+1]]);
        }
    }
    return contour;
};

Ideogram.prototype.getRegionPath = function(positions, chr, start, stop) {
    if (!positions.hasOwnProperty("names") || !positions.names.hasOwnProperty(chr)) {
        return null;
    }
    var cell = positions.positions[positions.names[chr]];
    var y0 = cell.y0;
    var y1 = cell.y1;
    var cellWidth = cell.x1 - cell.x0;
    start = start || 0;
    stop = stop || cell.end;
    var landmarks = [ cell.x0,
        cell.x0 + cellWidth * (cell.center/cell.end), 
        cell.x1];

    var r0 = landmarks[0] + cellWidth * (start/cell.end); 
    var r1 = landmarks[0] + cellWidth * (stop/cell.end);

    var contour = this.getContour(r0, r1, landmarks, 5);
    var points = [];
    for(var i =0; i<contour.length; i++) {
        var diff = contour[i];
        points.push([landmarks[0]+diff[0], y0+diff[1]]);
    }
    for(i=contour.length-1; i>-1; i--) {
        diff = contour[i];
        points.push([landmarks[0]+diff[0], y1-diff[1]]);
    }
    points.push([landmarks[0]+diff[0], y0+diff[1]]);
    return points;
};

Ideogram.prototype.calculatePositions = function(layout) {
    layout = layout || this.layout;
    if (!layout) {throw("No layout supplied");}
    var height = layout.height || 400;
    var width = layout.width || 800;
    var paddingY = (layout.padding && layout.padding.y) || layout.padding || 3;
    var rowHeight = Math.floor((height - 2*paddingY*layout.rows.length)/layout.rows.length);
    var positions = [];
    var names = {};
    for(var i=0; i<layout.rows.length; i++) {
        var row = layout.rows[i];
        var y0 = (rowHeight + 2*paddingY) * i + paddingY;
        var y1 = y0 + rowHeight - paddingY;
        var widths = this.getRowWidths(row, width, layout.max_row_extent); 
        var x0 = 0, x1 = 0;
        for(var j=0; j<row.cols.length; j++) {
            var col = JSON.parse(JSON.stringify(row.cols[j]));
            col.y0 = y0;
            col.y1 = y1;
            col.x0 = (x0 = x1);
            col.x1 = (x1 = x0 + widths[j]);
            positions.push(col);
            if (col.name) {
                names[col.name] = positions.length-1;
            }
        }
    }
    return {positions: positions, names: names};
};
