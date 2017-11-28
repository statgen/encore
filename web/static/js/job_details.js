/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "vars": "local" }] */
/* global create_gwas_plot, create_qq_plot, Ideogram, zoom_api_url, genome_build */

function init_job_tabs(job_id) {
    var tabkey = job_id + "_tab";
    $("ul.tabs li").click(function()
    {
        $("ul.tabs li").removeClass("active");
        $(this).addClass("active");
        $(".tab-content").css("z-index", "-1");
        var activeTab = $(this).attr("rel");
        $("#"+activeTab).css("z-index", "0");
        if (sessionStorage) {
            sessionStorage.setItem(tabkey, activeTab);
        }
    });
    var opentab = "";
    if (sessionStorage) {
        opentab = sessionStorage.getItem(tabkey);
        if (opentab)  {
            var findtab = $("ul.tabs li[rel='" + opentab + "']");
            if (findtab.length>0) {
                findtab.click();
            } else {
                opentab = "";
            }
        }
    }
    if (!opentab) {
        $("ul.tabs li:first").click();
    }
}

function init_job_resubmit_button(job_id, selector) {
    selector = selector || "button[name=resubmit_job]";
    $(selector).click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("link");
        document.location = url;
    });
}
function init_job_share_button(job_id, selector) {
    selector = selector || "button[name=share_job]";
    $(selector).click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("link");
        document.location = url;
    });
}

function init_job_delete_button(job_id, selector) {
    selector = selector || "button[name=delete_job]";
    $(selector).click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("action");
        $("button.delete-job").data("action", url);
        $("#deleteModal").modal();
    });
    $("#deleteModal button.delete-job").click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("action");
        $.ajax({
            url: url, 
            type: "DELETE",
            success: function() {
                document.location.reload();
            },
            error: function() {
                alert("Deletion request failed");
            }
        });
    });
}

function init_job_cancel_button(job_id, selector) {
    selector = selector || "button[name=cancel_job]";
    $(selector).click(function(evt)
    {
        evt.preventDefault();
        var url = $(evt.target).data("action");
        $("#cancelModal button.cancel-job").data("action", url);
        $("#cancelModal").modal();
    });
    $("#cancelModal button.cancel-job").click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("action");
        $.post(url).done( function() {
            alert("Cancellation in progress..."); 
            document.location = "../";
        }).fail(function(resp) {
            var msg = "Cancellation Failed";
            if(resp && resp.responseJSON && resp.responseJSON.error) {
                msg += " (" + resp.responseJSON.error + ")";
            }
            alert(msg); 
        });
    });
}

function init_manhattan(job_id, selector) {
    selector = selector || "#tab1";
    $.getJSON("/api/jobs/" + job_id + "/plots/manhattan").done(function(variants) {
        create_gwas_plot(selector, variants.variant_bins, variants.unbinned_variants, function(chrom, pos) {
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
            create_qq_plot("#tab2", data.data[0], data.header || {}, function(chrom, pos) {
                jumpToLocusZoom(job_id, chrom, pos);
            });
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

function getDataCols(cols, job_id) {
    var datacols = [];
    if (cols.indexOf("term")>-1) {
        datacols.push({data: "term", title:"Lookup"});
    }
    if (cols.indexOf("chrom")>-1) {
        datacols.push({data: null, title:"Chrom",
            orderData: [0,1],
            type: "num",
            render: function(data, type) {
                if (type=="sort"  && data.hasOwnProperty("chrom_sort")) {
                    return data.chrom_sort;
                }
                return data.chrom;
            },
            className: "dt-body-right"
        });
    }
    if (cols.indexOf("pos")>-1) {
        datacols.push({data: "pos", title:"Position", 
            render: function(data, type) {
                if (type=="display") {
                    if (data !== undefined && data !== null) {
                        return parseInt(data).toLocaleString();
                    } else {
                        return "";
                    }
                }
                return data;
            },
            orderData: [0,1],
            className: "dt-body-right"
        });
    }
    if (cols.indexOf("name")>-1) {
        datacols.push({data: "name", title:"Variant"});
    }
    if (cols.indexOf("pval")>-1) {
        datacols.push({data: "pval", title:"Best P-Value", 
            render: function(data, type) {
                if (type=="display") {
                    if (data !== undefined && data !== null) {
                        return data.toExponential(2);
                    } else {
                        return "";
                    }
                }
                return data;
            },
            className: "dt-body-right"
        });
    }
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
                if (data !== undefined && data !== null) {
                    var fn = "event.preventDefault();" + 
                        "jumpToLocusZoom(\"" + job_id + "\",\"" + row.chrom + "\"," + data + ")";
                    return "<a href='#' onclick='" + fn + "'>View</a>";
                } else {
                    return "";
                }
            },
            orderable: false,
            className: "dt-body-center"
        }
    );
    if (cols.indexOf("removeable")>-1) {
        datacols.push(
            {data: "term", title:"",
                className: "dt-body-center",
                render: function() {
                    return "<button type=\"button\" class=\"btn btn-default remove\">" + 
                        "<span class=\"glyphicon glyphicon-minus-sign\"></span></button>";
                },
                orderable: false
            }
        );
    }
    return datacols;
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
        var datacols = getDataCols(cols, job_id);
        var pvalcol = datacols.findIndex(function(x) {return x.data=="pval";});
        $(selector).DataTable( {
            data: data,
            columns: datacols,
            order: [[pvalcol, "asc"]],
            lengthChange: false,
            searching: false,
            dom: "rtip",
            stateSave: true
        });
        //$("#tophits").on("click","tr",function(event) {
        //  var data = table.row(this).data()
        //  jumpToLocusZoom(data.chrom, data.peak);
        //})
    }).fail(function() {
        $("ul.tabs li[rel='tab3'").remove();
    });
}

function init_chunk_progress(job_id, selector) {
    selector = selector || "#progress";
    $.getJSON("/api/jobs/" + job_id + "/progress").done(function(resp) {
        var chunks = resp.data || resp;
        if (chunks.length<1) {
            return;
        }
        $(selector).append("<h3>Progress</h3>");
        var ideo = new Ideogram(selector);
        chunks = chunks.map(function(x) {
            //x.fill = ["#3CA661","#66F297","#1D5932"][x.age] ;
            x.fill = ["#66F297","#3CA661","#3CA661"][x.age] ;
            return x;
        });
        ideo.setRegions(chunks);
        ideo.draw(genome_build);
        var boxcss = "display: inline-block; width: 1em; height:1em; margin: 0 5px; ";
        $(selector).append("<p style='text-align: center'><span style='" + boxcss + "background:#66F297'> </span>In Progress " + 
            "<span style='" + boxcss + "background:#3CA661'> </span>Completed</p>");
    });
}

function init_job_lookup(job_id) {
    var $lf = $("#lookup_form");
    var results = new LocalLookups(job_id);
    var dt;
    function drawResults() {
        if ( results.count() > 0 ) {
            if ( !dt ) {
                var $table = $("<table></table>").css("margin-top",10).width("100%").insertAfter($lf);
                dt = $table.DataTable({
                    data: results.get_lookups(), 
                    columns: getDataCols(results.get_columns() + ["removeable"], job_id),
                    lengthChange: false,
                    searching: false,
                    stateSave: true,
                    createdRow: function(row, data) {
                        if (!data.found) {
                            var msg = data.message || "Not Found";
                            var $tds = $("td", row);
                            $tds.slice(1,$tds.length-1).remove();
                            var $td = $("<td>").
                                attr("colspan", $tds.length-2).
                                text(msg);
                            $tds.first().after($td);
                        }
                    }
                });
                $table.on("click", "button.remove", function() {
                    if (confirm("Are you sure you want to delete this row?")) {
                        var $tr = $(this).closest("tr");
                        var row = dt.row($tr);
                        results.remove_lookup(row.data());
                        results.save_lookups();
                        drawResults();
                    }
                });
            } else {
                dt.clear().draw();
                dt.rows.add(results.get_lookups());
                dt.columns.adjust().draw();
            }
        }
    }
    drawResults();
    $lf.submit(function(e) {
        e.preventDefault();
        var term = $lf.find("#lookup").val();
        if(term && term.length) {
            result_lookup(term).then(function(resps) {
                resps.forEach(function(x) {results.add_lookup(x);});
                results.save_lookups();
                $lf.find("#lookup").val("");
                drawResults();
            });
        }
    });
    return job_id + 1;
}

function LocalLookups(job_id) {
    this.storageKey = job_id + "-lookups";
    if (!localStorage) {
        throw("No local storage available!");
    }
    this.results = this.load_lookups();
}

LocalLookups.prototype.load_lookups = function() {
    if (localStorage && localStorage.getItem(this.storageKey)) {
        this.results =  JSON.parse(localStorage.getItem(this.storageKey));
    } else {
        this.results =  [];
    }
    return this.results;
};

LocalLookups.prototype.get_lookups = function() {
    return this.results;
};

LocalLookups.prototype.save_lookups = function() {
    if (localStorage) {
        localStorage.setItem(this.storageKey, JSON.stringify(this.results));
    }
};

LocalLookups.prototype.add_lookup = function(result) {
    this.results.push(result);
};

LocalLookups.prototype.remove_lookup = function(result) {
    var idx = this.results.indexOf(result);
    this.results.splice(idx,1);
};

LocalLookups.prototype.get_columns = function() {
    return Object.keys(this.results[0]);
};

LocalLookups.prototype.count = function() {
    return this.results.length;
};

function single_lookup(x) {
    var msg = {chrom: x.chrom,
        start_pos: x.start,
        end_pos: x.end +1
    };
    if (x.error) {
        return $.when({term: x.term, chrom: null, pos: null, 
            pval: null, found: 0, message: x.error});
    }
    return $.getJSON(zoom_api_url, msg).then(function(resp) {
        if (resp.data.PVALUE && resp.data.PVALUE.length) {
            //return smallest p-value
            var min_pval = 1;
            var min_index = -1;
            for(var i=0; i<=resp.data.PVALUE.length; i++) {
                if (resp.data.PVALUE[i]<min_pval) {
                    min_pval = resp.data.PVALUE[i];
                    min_index = i;
                }
            }
            return {term: x.term, 
                chrom: resp.data.CHROM[min_index],
                pos: resp.data.BEGIN[min_index],
                pval: parseFloat(resp.data.PVALUE[min_index]),
                found: 1
            };
        }
        //no results found
        return {term: x.term, chrom: null, pos:null, pval: null, 
            found: 0, message: "No p-value in range"};
    });
}
function result_lookup(term) {
    var position_url = "//portaldev.sph.umich.edu/api/v1/annotation/omnisearch/";
    var req = {
        q: term,
        build: genome_build
    };
    return $.getJSON(position_url, req).then(function(x) {
        if (x.data && x.data.length) {
            if (x.build && x.build=="grch38") {
                x.data = x.data.map(function(x) {x.chrom="chr" + x.chrom; return(x);});
            }
            var lookups = x.data.map(single_lookup);
            return $.when.apply($, lookups).then(function() {
                var lookups = Array.prototype.slice.call(arguments);
                return lookups;
            });
        } else {
            alert("Position lookup failed");
            return $.when([]);
        }
    });
}

function jumpToLocusZoom(job_id, chr, pos) {
    if (job_id && chr && pos) {
        pos = parseInt(pos);
        var region = chr + ":" + (pos-100000) + "-" + (pos+100000);
        document.location.href = "/jobs/" + job_id + "/locuszoom/" + region;
    }
}

function init_editform(job_id, job_api_url) {
    var edit_icon = $("<span>", {class: "glyphicon glyphicon-pencil", "aria-hidden": "true"});
    $("#job_name_title").append($("<sup>").append($("<span>", {class: "label label-edit"}).append($("<a>", { class:"edit-job-modal"}).append(edit_icon))));
    $("#editModal").find("form").on("keyup keypress", function(e) {
        var keyCode = e.keyCode || e.which;
        if (keyCode === 13) { 
            e.preventDefault();
            return false;
        }
    });
    $("a.edit-job-modal").click(function(evt) {
        evt.preventDefault();
        $.getJSON(job_api_url).then(function(resp) {
            $("#editModal").find("#job_name").val(resp.name);
            $("#editModal").on("shown.bs.modal", function() {
                $("#editModal").find("#job_name").focus();
            });
            $("#editModal").modal();
        });
    });
    $("button.edit-job-save").click(function(evt) {
        evt.preventDefault();
        var new_name = $("#editModal").find("#job_name").val();
        $.post(job_api_url, {"name": new_name}).done( function() {
            $("#job_name_title").contents().first().replaceWith(new_name);
            $("#editModal").modal("hide");
        }).fail(function() {
            alert("Update failed");
        });
    });
}

function init_queue_info(selector) {
    selector = selector || "#queue_info";
    var $ele = $(selector);
    var url = $ele.data("url");
    $.getJSON(url).done(function(resp) {
        var text = "Queue status: ";
        text += (resp.running || 0) + " job(s) running. ";
        text += (resp.queued || 0) + " job(s) queued. ";
        if(resp.position) {
            text += "Position in queue: " + resp.position;
        }
        $(selector).addClass("queue-info bg-info");
        $(selector).append(text);
    });
}
