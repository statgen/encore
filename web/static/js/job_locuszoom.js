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

	var apiBase = "/api/lz/";
    data_sources.add("epacts", new EpactsDS)
      .add("ld", ["LDEP", apiBase + "ld-"])
      .add("gene", ["GeneLZ", { url: apiBase + "gene", params: {source: 2} }])
      .add("recomb", ["RecombLZ", { url: apiBase + "recomb", params: {source: 15} }])
      .add("constraint", ["GeneConstraintLZ", { url: "constraint" }])
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
    "state": {},
    "width": 1000,
    "height": 500,
    "resizable": "responsive",
    "panel_boundaries": false,
    "aspect_ratio": 1.7777777777777777,
    "min_region_scale": 20000,
    "max_region_scale": 4000000,
    "panels": [{
        "id": "positions",
        "title": "",
        "width": 800,
        "height": 225,
        "origin": {
            "x": 0,
            "y": 0
        },
        "min_width": 400,
        "min_height": 200,
        "proportional_width": 1,
        "proportional_height": 0.5,
        "proportional_origin": {
            "x": 0,
            "y": 0
        },
        "margin": {
            "top": 35,
            "right": 50,
            "bottom": 40,
            "left": 50
        },
        "inner_border": "rgba(210, 210, 210, 0.85)",
        "axes": {
            "x": {
                "label_function": "chromosome",
                "label_offset": 32,
                "tick_format": "region",
                "extent": "state"
            },
            "y1": {
                "label": "-log10 p-value",
                "label_offset": 28
            },
            "y2": {
                "label": "Recombination Rate (cM/Mb)",
                "label_offset": 40
            }
        },
        "interaction": {
            "drag_background_to_pan": true,
            "drag_x_ticks_to_scale": true,
            "drag_y1_ticks_to_scale": true,
            "drag_y2_ticks_to_scale": false,
            "scroll_to_zoom": true,
            "x_linked": true
        },
        "data_layers": [{
            "id": "recomb",
            "type": "line",
            "fields": ["recomb:position", "recomb:recomb_rate"],
            "z_index": 1,
            "style": {
                "stroke": "#0000FF",
                "stroke-width": "1.5px"
            },
            "x_axis": {
                "field": "recomb:position"
            },
            "y_axis": {
                "axis": 2,
                "field": "recomb:recomb_rate",
                "floor": 0,
                "ceiling": 100
            },
            "transition": {
                "duration": 200
            }
        }, {
            "id": "positions",
            "type": "scatter",
            "point_shape": "circle",
            "point_size": {
                "scale_function": "if",
                "field": "ld:isrefvar",
                "parameters": {
                    "field_value": 1,
                    "then": 80,
                    "else": 40
                }
            },
            "color": [{
                "scale_function": "if",
                "field": "ld:isrefvar",
                "parameters": {
                    "field_value": 1,
                    "then": "#9632b8"
                }
            }, {
                "scale_function": "numerical_bin",
                "field": "ld:state",
                "parameters": {
                    "breaks": [0, 0.2, 0.4, 0.6, 0.8],
                    "values": ["#357ebd", "#46b8da", "#5cb85c", "#eea236", "#d43f3a"]
                }
            }, "#B8B8B8"],
            "fields": ["epacts:MARKER_ID", "epacts:MAF", "epacts:CHROM", 
                "epacts:END", "epacts:BEGIN", "epacts:PVALUE|neglog10", 
                "epacts:PVALUE|scinotation", "epacts:PVALUE", "epacts:BETA", 
                "epacts:NS", "ld:state", "ld:isrefvar"],
            "id_field": "epacts:MARKER_ID",
            "z_index": 2,
            "x_axis": {
                "field": "epacts:BEGIN"
            },
            "y_axis": {
                "axis": 1,
                "field": "epacts:PVALUE|neglog10",
                "floor": 0,
                "upper_buffer": 0.1,
                "min_extent": [0, 10]
            },
            "transition": {
                "duration": 200
            },
            "highlighted": {
                "onmouseover": "on",
                "onmouseout": "off"
            },
            "selected": {
                "onclick": "toggle_exclusive",
                "onshiftclick": "toggle"
            },
            "tooltip": {
                "closable": false,
                "show": {
                    "or": ["highlighted"]
                },
                "hide": {
                    "and": ["unhighlighted"]
                },
                "html": 
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
        }]
    }, {
        "id": "genes",
        "width": 800,
        "height": 225,
        "origin": {
            "x": 0,
            "y": 225
        },
        "min_width": 400,
        "min_height": 112.5,
        "proportional_width": 1,
        "proportional_height": 0.5,
        "proportional_origin": {
            "x": 0,
            "y": 0.5
        },
        "margin": {
            "top": 20,
            "right": 50,
            "bottom": 20,
            "left": 50
        },
        "axes": {},
        "interaction": {
            "drag_background_to_pan": true,
            "scroll_to_zoom": true,
            "x_linked": true
        },
        "data_layers": [{
            "id": "genes",
            "type": "genes",
            "fields": ["gene:gene", "constraint:constraint"],
            "id_field": "gene_id",
            "highlighted": {
                "onmouseover": "on",
                "onmouseout": "off"
            },
            "selected": {
                "onclick": "toggle_exclusive",
                "onshiftclick": "toggle"
            },
            "transition": {
                "duration": 200
            },
            "tooltip": {
                "closable": true,
                "show": {
                    "or": ["highlighted", "selected"]
                },
                "hide": {
                    "and": ["unhighlighted", "unselected"]
                },
                "html": "<h4><strong><i>{{gene_name}}</i></strong></h4><div style=\"float: left;\">Gene ID: <strong>{{gene_id}}</strong></div><div style=\"float: right;\">Transcript ID: <strong>{{transcript_id}}</strong></div><div style=\"clear: both;\"></div><table><tr><th>Constraint</th><th>Expected variants</th><th>Observed variants</th><th>Const. Metric</th></tr><tr><td>Synonymous</td><td>{{exp_syn}}</td><td>{{n_syn}}</td><td>z = {{syn_z}}</td></tr><tr><td>Missense</td><td>{{exp_mis}}</td><td>{{n_mis}}</td><td>z = {{mis_z}}</td></tr><tr><td>LoF</td><td>{{exp_lof}}</td><td>{{n_lof}}</td><td>pLI = {{pLI}}</td></tr></table><div style=\"width: 100%; text-align: right;\"><a href=\"http://exac.broadinstitute.org/gene/{{gene_id}}\" target=\"_new\">More data on ExAC</a></div>"
            }
        }]
    }]
};

    lzplot = LocusZoom.populate("#locuszoom", data_sources, layout);

});


