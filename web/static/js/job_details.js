
$(document).ready(function()
{
    var job_id = /^\/jobs\/(.*)$/.exec(window.location.pathname)[1];

    $("ul.tabs li").click(function()
    {
        $("ul.tabs li").removeClass("active");
        $(this).addClass("active");
        $(".tab-content").css("z-index", "-1");
        var activeTab = $(this).attr("rel");
        $("#"+activeTab).css("z-index", "0");
    });
    $("ul.tabs li:first").click();

    $("#back_arrow").click(function()
    {
        window.location = history.go(-1);
    });

    $("button[name=cancel_job]").click(function()
    {
        var xhr = new XMLHttpRequest();
        xhr.addEventListener("load", function(ev)
        {
           location.reload();
        }, false);

        xhr.addEventListener("error", function() { alert("Request Failed"); }, false);
        xhr.open("POST", "/api/jobs/" + job_id + "/cancel_request");
        xhr.send();
    });

    var data_sources = new LocusZoom.DataSources();
    var myds = LocusZoom.Data.Source.extend();
    myds.prototype.getURL = function (state)
    {
        return "/api/jobs/" + job_id + "/plots/zoom?chrom=" + state.chr + "&start_pos=" + state.start + "&end_pos=" + state.end;
    };
    data_sources.add("epacts", new myds());


    var layout =
    {
        width: 1000,
        height: 500,
        resizable: "responsive",
        panels:
        {
            association:
            {
                margin: { top: 40, right: 50, bottom: 40, left: 50 },
                axes:
                {
                    x:
                    {
                        label: "Region",
                        /*"label_function": function (a)
                         {
                         return "Chromosome ";
                         },*/
                        label_offset: 30,
                        extent: "state"
                    },
                    y1:
                    {
                        label: "P Value (-log10)",
                        label_offset: 30,
                        extent: "state"
                    }
                },
                data_layers:
                {
                    association:
                    {
                        type: "scatter",
                        fields: ["epacts:MARKER_ID", "epacts:MAF", "epacts:CHROM", "epacts:END", "epacts:BEGIN", "epacts:PVALUE|neglog10"],
                        id_field: "epacts:MARKER_ID",
                        x_axis: {field: "epacts:BEGIN"},
                        y_axis: {field: "epacts:PVALUE|neglog10"},
                        /*highlighted:
                         {
                         onmouseover: "on",
                         onmouseout: "off"
                         },
                         selected:
                         {
                         onclick: "toggle_exclusive",
                         onshiftclick: "toggle"
                         },*/
                        tooltip:
                        {
                            closable: true,
                            show: { or: ["highlighted", "selected"] },
                            hide: { and: ["unhighlighted", "unselected"] },
                            html:
                            "<div style='text-align: right'>"
                            //+ "<strong>{{epacts:MARKER_ID}}</strong><br>"
                            + "Chrom: <strong>{{epacts:CHROM}}</strong><br/>"
                            + "Pos Beg: <strong>{{epacts:BEGIN}}</strong><br/>"
                            + "Pos End: <strong>{{epacts:END}}</strong><br/>"
                            + "P Value: <strong>{{epacts:PVALUE|neglog10}}</strong><br>"
                            + "MAF: <strong>{{epacts:MAF}}</strong><br/>"
                            + "</div>"
                        }
                    }
                }
            }
        }
    };

    var plot = LocusZoom.populate("#locuszoom", data_sources, layout);

    $.getJSON("/api/jobs/" + job_id + "/plots/manhattan").done(function(variants)
    {
        create_gwas_plot("#tab1", variants.variant_bins, variants.unbinned_variants, function(chrom, pos, ref, alt)
        {
            console.log(chrom, pos, ref, alt);

            plot.applyState({ chr: chrom, start: pos - 15000, end: pos + 15000 });

            ///api/jobs/f3ae4bd2-cc3c-4858-8f20-d96bc198c316/plots/zoom?chrom=11&start_pos=20889929&end_pos=20899929
        });

    });
    $.getJSON("/api/jobs/" + job_id + "/plots/qq").done(function(data)
    {
        /*_.sortBy(_.pairs(data.overall.gc_lambda)).forEach(function(d)
         {
         $('.gc-control').append('<br>GC Lambda ' + d[0] + ': ' + d[1].toFixed(3));
         });*/
        create_qq_plot("#tab2", data);
    });
});