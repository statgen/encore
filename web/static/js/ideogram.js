/ global d3 */

var Ideogram = function(selector) {
    this.selector = selector;
    this.svg = d3.select(selector).append("svg")
        .attr("width",800)
        .attr("height", 500);
    this.svg.append("g")
        .attr("class", "chromosome");
    this.svg.append("g")
        .attr("class", "outline");

    if (d3.tip) {
        this.tooltip = d3.tip()
            .attr("class", "d3-tip")
            .offset([-10,0])
            .html(function(d) {
                return d.tooltip;
            });
        this.svg.call(this.tooltip);
    }

    this.regions = [
        {chrom:"chr1", start:115000000, stop:135000000},
        {chrom:"chr4", start:11500000, stop:53500000, fill:"#66F297"},
        {chrom:"chr14", start:0, stop:2e7, fill:"#1D5932"},
        {chrom:"chr14", start:2e7, stop:4e7, fill:"#3CA661"},
        {chrom:"chr14", start:4e7, stop:5e7, fill:"#66F297", tooltip:"new"}
    ];
};

Ideogram.prototype.draw = function() {
    var layout = this.getLayout(this.chrs_hg19);
    this.drawOutlines(layout);
    this.drawRegions(layout, this.regions);
};

Ideogram.prototype.setRegions = function(regions) {
    this.regions = regions;
};

Ideogram.prototype.findElement = function(obj, type) {
    var ele = [];
    if (Array.isArray(obj)) {
        obj.forEach(function(x) {
            if (x.type && x.type==type) {
                ele.push(x);
            }
            ele = ele.concat(Ideogram.prototype.findElement(x, type));
        });
    } else if (obj !==null && typeof obj === "object") {
        Object.keys(obj).forEach(function(k) {
            var x = obj[k];
            if (x.type && x.type==type) {
                ele.push(x);
            }
            ele = ele.concat(Ideogram.prototype.findElement(x, type));
        });
    }
    return ele;
};

Ideogram.prototype.toPointsString = function(d) {
    return d.points.map(function(d) {return [d[0].toFixed(2), d[1].toFixed(2)].join(",");}).join(" ");
};

Ideogram.prototype.drawOutlines = function(layout, opts) {
    opts = opts || {fill: "none", stroke: "black", "stroke-width": 1.5};
    var regions = []; 
    var chrs = this.findElement(layout, "chr");
    chrs.forEach(function(x) {
        regions.push({points: this.getRegionPath(layout, x.name)});
    }.bind(this));
    this.svg.select("g.outline").selectAll("polygon.chroutline")
        .data(regions)
        .enter().append("polygon")
        .attr("class"," chroutline")
        .attr("points", this.toPointsString)
        .attr(opts)
};

Ideogram.prototype.drawRegions = function(layout, regions, opts) {
    opts = opts || {fill: "#c8c8c8"};
    var polys = []; 
    var hasToolTips = false;
    for(var i=0; i<regions.length; i++) {
        polys.push({points: this.getRegionPath(layout,
            regions[i].chrom, 
            regions[i].start,
            regions[i].stop),
        fill: regions[i].fill,
        tooltip: regions[i].tooltip});
        hasToolTips |= !!(regions[i].tooltip);
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
            }.bind(this))
            r.on("mouseout", this.tooltip.hide);
        } else {
            console.warning("Tooltips not available (include d3.tip)")
        }
    }
};

Ideogram.prototype.chrs_hg19 = [
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


Ideogram.prototype.getLayout = function(chrs, options) {
    options = options || {};
    var rows = [];
    var maxextent = 0;
    var lookup = {};
    var drawSex = false;
    var add2col = function(i, a,b) {
        var cols = [];
        var rowextent = a.end + b.end;
        if (rowextent > maxextent) {maxextent = rowextent;}
        cols.push({type: "label", width: "40px", text: a.chr});
        cols.push({type: "chr", name: a.chr, center: a.center, end: a.end});
        cols.push({type: "gap", min_width: "25px"});
        cols.push({type: "chr", name: b.chr, center: b.center, end: b.end});
        cols.push({type: "label", width: "40px", text: b.chr});
        rows.push({cols: cols});
        lookup[a.chr] = [i, 1];
        lookup[b.chr] = [i, 3];
    };
    for(var i=0; i<11; i++) {
        var a = chrs[i];
        var b = chrs[21-i];
        add2col(i,a,b);
    }
    if (drawSex) {
        add2col(i, chrs[22], chrs[23]);
    }
    return {rows: rows, max_row_extent: maxextent,
        corner_ease: 5, chr: lookup};
};

Ideogram.prototype.getRowWidths = function(row, width, scale) {
    var fixed = 0;
    var gap = -1;
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
            gap = idx;
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
    if (gap > -1 && remainWidth>0) {
        widths[gap] = remainWidth; 
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

Ideogram.prototype.getRegionPath = function(layout, chr, start, stop) {
    var height = layout.height || 400;
    var width = layout.width || 800;
    var paddingY = (layout.padding && layout.padding.y) || layout.padding || 3;
    var cellPos = layout.chr[chr];
    var row = layout.rows[cellPos[0]];
    var cell = row.cols[cellPos[1]];
    var rowHeight = Math.floor((height - 2*paddingY*layout.rows.length)/layout.rows.length);
    var y0 = (rowHeight + 2*paddingY) * cellPos[0] + paddingY;
    var y1 = y0 + rowHeight - paddingY;
    var widths = this.getRowWidths(row, width, layout.max_row_extent); 
    var cellWidth = widths[cellPos[1]];
    start = start || 0;
    stop = stop || cell.end;
    var landmarks = [ widths.slice(0, cellPos[1]).reduce(function(a,b) {return a+b;}) ];
    landmarks[1] = landmarks[0] + cellWidth * (cell.center/cell.end); 
    landmarks[2] = landmarks[0] + cellWidth;

    var x0 = landmarks[0] + cellWidth * (start/cell.end); 
    var x1 = landmarks[0] + cellWidth * (stop/cell.end);

    var contour = this.getContour(x0, x1, landmarks, 5);
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

