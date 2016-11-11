
/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "vars": "local" }] */
/* global create_gwas_plot, create_qq_plot, Ideogram */

function init_job_tabs() {
    $("ul.tabs li").click(function()
    {
        $("ul.tabs li").removeClass("active");
        $(this).addClass("active");
        $(".tab-content").css("z-index", "-1");
        var activeTab = $(this).attr("rel");
        $("#"+activeTab).css("z-index", "0");
    });
    $("ul.tabs li:first").click();
}

function init_job_cancel_button(job_id, selector) {
    selector = selector || "button[name=cancel_job]";
    $(selector).click(function()
    {
        var xhr = new XMLHttpRequest();
        xhr.addEventListener("load", function()
        {
            location.reload();
        }, false);

        xhr.addEventListener("error", function() { alert("Request Failed"); }, false);
        xhr.open("POST", "/api/jobs/" + job_id + "/cancel_request");
        xhr.send();
    });
}

function init_manhattan(job_id, selector) {
    selector = selector || "#tab1";
    $.getJSON("/api/jobs/" + job_id + "/plots/manhattan").done(function(variants)
    {
        create_gwas_plot(selector, variants.variant_bins, variants.unbinned_variants, function(chrom, pos)
        {
            jumpToLocusZoom(job_id, chrom, pos);
        });

    });
}

function init_qqplot(job_id, selector, data_url) {
    selector = selector || "#tab2";
    data_url = data_url || "/api/jobs/" + job_id + "/plots/qq"; 
    $.getJSON(data_url).done(function(data)
    {
        /*_.sortBy(_.pairs(data.overall.gc_lambda)).forEach(function(d)
         {
         $('.gc-control').append('<br>GC Lambda ' + d[0] + ': ' + d[1].toFixed(3));
         });*/
        if (data.data) {
            create_qq_plot("#tab2", data.data[0], data.header || {});
        } else {
            //old style
            var reformat = {layers:[]};
            data.forEach(function(d) {
                var layer = {"variant_bins": d.qq, "unbinned_variants": []};
                layer.maf_range = d.maf_range;
                layer.count = d.count;
                reformat.layers.push(layer);

            });
            create_qq_plot("#tab2", reformat, {});
        }
    });
}

function init_tophits(job_id, selector, data_url) {
    selector = selector || "#tophits";
    data_url = data_url|| "/api/jobs/" + job_id + "/tables/top";
    $.getJSON(data_url).done(function(data) {
        var header = data.header || {};
        data = data.data || data;
        var cols;
        if ( header.cols ) {
            cols = header.cols;
        } else {
            cols = Object.keys(data[0]);
        }
        var chrpos = {};
        var i=0;
        for(i=1; i<=22; i++) {
            chrpos[i.toString()] = i;
        }
        chrpos["X"] = i++;
        chrpos["Y"] = i++;
        chrpos["XY"] = i++;
        chrpos["MT"] = i++;
        var chr;
        for(var j=0; j<data.length; j++) {
            chr = data[j].chrom.replace("chr","");
            if (chrpos[chr] !== undefined) {
                data[j].chrom_sort = chrpos[chr];
            } else {
                data[j].chrom_sort = chrpos[chr] = i++;
            }
        }
        var datacols = [
            {data: null, title:"Chrom",
                orderData: [0,1],
                type: "num",
                render: function(data, type) {
                    if (type=="sort") {
                        return data.chrom_sort;
                    }
                    return data.chrom;
                },
                className: "dt-body-right"
            },
            {data: "pos", title:"Position", 
                render: function(data, type) {
                    if (type=="display") {
                        return data.toLocaleString();
                    }
                    return data;
                },
                orderData: [0,1],
                className: "dt-body-right"
            },
            {data: "name", title:"Variant"},
            {data: "pval", title:"Best P-Value", 
                render: function(data, type) {
                    if (type=="display") {
                        return data.toExponential(2);
                    }
                    return data;
                },
                className: "dt-body-right"
            }
        ];
        if (cols.indexOf("sig_count")>-1) {
            datacols.push(
                {data: "sig_count", title:"# Significant",
                    className: "dt-body-center"
                }
            );
        }
        if (cols.indexOf("gene")>-1) {
            datacols.push(
                {data: "gene", title:"Nearest gene",
                    className: "dt-body-center"
                }
            );
        }
        datacols.push(
            {data: "pos", title:"Plot",
                render:function(data, type, row) {
                    var fn = "event.preventDefault();" + 
                        "jumpToLocusZoom(\"" + job_id + "\",\"" + row.chrom + "\"," + data + ")";
                    return "<a href='#' onclick='" + fn + "'>View</a>";
                },
                orderable: false,
                className: "dt-body-center"
            }
        );
        $(selector).DataTable( {
            data: data,
            columns: datacols,
            order: [[3, "asc"]],
            lengthChange: false,
            searching: false,
            dom: "rtip"
        });
        //$("#tophits").on("click","tr",function(event) {
        //  var data = table.row(this).data()
        //  jumpToLocusZoom(data.chrom, data.peak);
        //})
    }).fail(function() {
        $("ul.tabs li[rel='tab3'").remove();
    });
}

function bin_chunks_by_chr_and_age(chunks, now) {
    var getAgeGroup = function(a) {
        var diffMin = (now-a)/1000/60;
        if (diffMin < 10) {
            return 0;
        } else if (diffMin < 60) {
            return 1;
        } else {
            return 2;
        }
    };
    function startSort(a,b) {
        return a.start - b.start;
    }
    //http://machinesaredigging.com/2014/04/27/binary-insert-how-to-keep-an-array-sorted-as-you-insert-data-in-it/
    function binaryInsert(element, array, comp, start, end) {
        start = (typeof start != "undefined") ? start : 0;
        end = (typeof end != "undefined") ? end : array.length-1;
        var pivot = start + Math.floor((end - start) / 2);
        if (array.length==0) {array.push(element); return;}
        if (comp(element, array[end])>0) {
            array.splice(end+1, 0, element); return;
        }
        if (comp(element, array[start])<0) {
            array.splice(start, 0, element); return;
        }
        if (start >= end) {return;}
        var c = comp(array[pivot], element);
        if (c>0) {
            binaryInsert(element, array, comp, start, pivot-1);
        } else {
            binaryInsert(element, array, comp, pivot+1, end);
        }
        return;
    }
    var bins = {};
    var group;
    chunks.forEach(function(chunk) {
        var modified = new Date(chunk.modified);
        var age = getAgeGroup(modified);
        group = "" + chunk.chr + "-" + age;
        if (!(group in bins)) {
            bins[group] = {chrom: "chr" + chunk.chr, age: age,
                vals: [{start: chunk.start, stop: chunk.stop, 
                    modified: chunk.modified}]};
        } else {
            var g = bins[group];
            binaryInsert(chunk, g.vals, startSort);
        }
    });
    return bins;
}

function collapse_chunk_bins(bins) {
    var coll = [];
    function add(chr, age, start, stop, oldest, newest) {
        coll.push({chrom: chr, age: age, start: start, stop:stop,
            oldest: oldest, newest: newest});
    }
    var start;
    var stop;
    var oldest;
    var newest;
    function reset(val) {
        start = val.start;
        stop = val.stop;
        oldest = val.modified;
        newest = val.modified;
    }
    function extend(val) {
        stop = val.stop;
        oldest = (val.modified<oldest) ? val.modified : oldest;
        newest = (val.modified>newest) ? val.modified : newest;
    }
    Object.keys(bins).forEach(function(k) {
        var bin = bins[k];
        reset(bin.vals[0]);
        for(var i=1; i<bin.vals.length; i++) {
            if (bin.vals[i].start != stop+1) {
                add(bin.chrom, bin.age, start, stop, oldest, newest);
                reset(bin.vals[i]);
            } else {
                extend(bin.vals[i]);
            }
        }
        add(bin.chrom, bin.age, start, stop, oldest, newest);
    });
    return coll;
}

function init_chunk_progress(job_id, selector) {
    selector = selector || "#progress";
    $.getJSON("/api/jobs/" + job_id + "/chunks").done(function(resp) {
        var now = (resp.now && new Date(resp.now)) || new Date();
        chunks = resp.data || resp;
        if (chunks.length<1) {
            return;
        }
        var x = bin_chunks_by_chr_and_age(chunks, now);
        var chunks = collapse_chunk_bins(x);
        $(selector).append("<h3>Progress</h3>");
        var ideo = new Ideogram(selector);
        chunks = chunks.map(function(x) {
            //x.fill = ["#3CA661","#66F297","#1D5932"][x.age] ;
            x.fill = ["#66F297","#3CA661","#3CA661"][x.age] ;
            return x;
        });
        ideo.setRegions(chunks);
        ideo.draw();
    });
}

function jumpToLocusZoom(job_id, chr, pos) {
    if (job_id && chr && pos) {
        var region = chr + ":" + (pos-100000) + "-" + (pos+100000);
        document.location.href = "/jobs/" + job_id + "/locuszoom/" + region;
    }
}


