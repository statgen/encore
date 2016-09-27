var lzplot;
$(document).ready(function() {

    var data_sources = new LocusZoom.DataSources();
    var EpactsDS = LocusZoom.Data.Source.extend();
    EpactsDS.prototype.getURL = function (state)
    {
        return "/api/jobs/" + job_id + "/plots/zoom?chrom=" + state.chr + "&start_pos=" + state.start + "&end_pos=" + state.end;
    };
	var EpactsLD = LocusZoom.Data.Source.extend(function(init) {
		this.parseInit(init);
	}, "LDEP", LocusZoom.Data.LDSource);
	EpactsLD.prototype.findMergeFields = function() {
		return {
			id: "epacts:MARKER_ID",
			position: "epacts:BEGIN",
			pvalue: "epacts:PVALUE|neglog10"
		}
	};	
	var apiBase = "http://portaldev.sph.umich.edu/api/v1/";
    data_sources.add("epacts", new EpactsDS)
      .add("ld", ["LDEP" ,apiBase + "pair/LD/"])
      .add("gene", ["GeneLZ", { url: apiBase + "annotation/genes/", params: {source: 2} }])
      .add("recomb", ["RecombLZ", { url: apiBase + "annotation/recomb/results/", params: {source: 15} }])
      .add("constraint", ["GeneConstraintLZ", { url: "http://exac.broadinstitute.org/api/constraint" }])
      .add("sig", ["StaticJSON", [{ "x": 0, "y": 4.522 }, { "x": 2881033286, "y": 4.522 }] ]);

	LocusZoom.TransformationFunctions.set("scinotation", function(x) {
		var log;
		if (x=="NA") {return "-"};
		x = parseFloat(x);
		if (Math.abs(x) > 1){
			log = Math.ceil(Math.log(x) / Math.LN10);
		} else {
			log = Math.floor(Math.log(x) / Math.LN10);
		}
		if (Math.abs(log) <= 3){
			return x.toFixed(3);
		} else {
			return x.toExponential(2).replace("+", "").replace("e", " × 10^");
		}
	});

	var layout = {
		width:1000,
		height:500,
		controls: false,
		resizable: "responsive",
		panel_boundaries: false,
		panels: {
			positions: {
				controls: false,
				title: "",
				data_layers: {
					positions: {
						fields: ["epacts:MARKER_ID", "epacts:MAF", "epacts:CHROM", "epacts:END",
							 "epacts:BEGIN", "epacts:PVALUE|neglog10", "epacts:PVALUE|scinotation",
							 "epacts:PVALUE", "epacts:BETA", "epacts:NS", "ld:state"],
						id_field: "epacts:MARKER_ID",
						x_axis: {field: "epacts:BEGIN"},
						y_axis: {field: "epacts:PVALUE|neglog10"},
						tooltip: {
                            closable: true,
                            show: { or: ["highlighted", "selected"] },
                            hide: { and: ["unhighlighted", "unselected"] },
                            html:
                            "<div style='text-align: right'>"
                            + "<strong>{{epacts:MARKER_ID}}</strong><br>"
                            + "Chrom: <strong>{{epacts:CHROM}}</strong><br/>"
                            + "Pos: <strong>{{epacts:BEGIN}}</strong><br/>"
                            + "P Value: <strong>{{epacts:PVALUE|scinotation}}</strong><br>"
                            + "MAF: <strong>{{epacts:MAF}}</strong><br/>"
                            + "BETA: <strong>{{epacts:BETA}}</strong><br/>"
                            + "N: <strong>{{epacts:NS}}</strong><br/>"
                            + "</div>"
                        }
					}
				}
			},
			genes: {
				controls: false
			}
		}
	};
	layout = LocusZoom.mergeLayouts(layout, LocusZoom.StandardLayout);
    var oldlayout =
    {
        width: 1000,
        height: 500,
        resizable: "responsive",
        panels:
        {
            positions:
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
                        
                    }
                }
            }
        }
    };

    lzplot = LocusZoom.populate("#locuszoom", data_sources, layout);

});


