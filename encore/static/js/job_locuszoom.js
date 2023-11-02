/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "vars": "local" }] */
/* global LocusZoom, job_id, api_url, genome_build */

var lzplot;
$(document).ready(function() {

    var data_sources = new LocusZoom.DataSources();
    var EpactsDS = LocusZoom.Adapters.get("AssociationLZ");
    EpactsDS.prototype.getURL = function (state) {
        return api_url + "?chrom=" + state.chr + "&start_pos=" + state.start + "&end_pos=" + state.end;
    };
    EpactsDS.prototype.preGetData = function(state, fields, outnames, trans) {
        const id_field = this.params.id_field || 'id';
        return {fields: fields, outnames:outnames, trans:trans};
    }

    var apiBase = "/api/lz/";
    data_sources.add("epacts", new EpactsDS({url: api_url, params: {
        id_field: "MARKER_ID", position_field: "BEGIN"}}))
      .add("sig", ["StaticJSON", [{ "x": 0, "y": 4.522 }, { "x": 2881033286, "y": 4.522 }] ]);

    if (Object.keys(ld_info).length !== 0) {
        data_sources.add("ld",
            ["LDServer", {
                url: apiBase + "ld/",
                params: {
                    source: ld_info.panel,
                    population: ld_info.population,
                    build: ld_info.build
                }
            }]);
        data_sources.get("ld").getURL = function(state, chain, fields) {
            // The default implementation mangles the refvar (removes the "chr")
            // That might be necessary for 1000G ref, but doesn't work for TOPMed
            const build = state.genome_build || this.params.build || 'GRCh37';
            const source = state.ld_source || this.params.source || '1000G';
            const population = state.ld_pop || this.params.population || 'ALL';
            const method = this.params.method || 'rsquare';

            let refVar = this.getRefvar(state, chain, fields);
            chain.header.ldrefvar = refVar;


            varmodchr = '';
            if (refVar.includes("_rs")) {
                varmodchr= refVar.substring(0, myString.indexOf("_rs"));
            } else {
                varmodchr=refVar;
            }


            return  [
                this.url, 'genome_builds/', build, '/references/', source, '/populations/', population, '/variants',
                '?correlation=', method,
                '&variant=', encodeURIComponent(varmodchr),
                '&chrom=', encodeURIComponent(state.chr),
                '&start=', encodeURIComponent(state.start),
                '&stop=', encodeURIComponent(state.end),
            ].join('');
        };
        data_sources.get("ld").findMergeFields = function() {
            return {
                id: "epacts:MARKER_ID",
                position: "epacts:BEGIN",
                pvalue: "epacts:PVALUE|neglog10"
            };
        };
    }

    if (genome_build == "GRCh37") {
        data_sources
          .add("gene", ["GeneLZ", { url: apiBase + "gene", params: {source: 2} }])
          .add("recomb", ["RecombLZ", { url: apiBase + "recomb", params: {source: 15} }]);
    } else if (genome_build == "GRCh38") {
        data_sources
          .add("gene", ["GeneLZ", { url: apiBase + "gene", params: {source: 1 } }]);
          //.add("recomb", ["RecombLZ", { url: apiBase + "recomb", params: {source: 15} }]);
    }

    data_sources.get("gene").getURL = function(state, chain, fields) {
        var ns = Object.assign({}, state);
        if (ns.chr) {
            ns.chr = ns.chr.replace("chr","");
        }
        return LocusZoom.Adapters.get("GeneLZ").prototype.getURL.call(this, ns, chain, fields);
    };



    LocusZoom.TransformationFunctions.add("scinotation2", function(x) {
        var log;
        if (x=="NA") {return "-";}
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

    LocusZoom.TransformationFunctions.add("sig3", function(x) {
        if (x=="NA") {return "-";}
        var x = parseFloat(x);
        return x.toPrecision(3);
    });


    function getlayout(avail_fields, build) {
        var has_maf = avail_fields.indexOf("MAF") !== -1;
        var has_beta = avail_fields.indexOf("BETA") !== -1;
        var has_ns = avail_fields.indexOf("NS") !== -1;
        var has_ld = Object.keys(ld_info).length !== 0;
        var fields = ["epacts:MARKER_ID", "epacts:CHROM",
            "epacts:BEGIN", "epacts:PVALUE|neglog10",
            "epacts:PVALUE|scinotation2", "epacts:PVALUE"];
        var tooltipText = "<div style='text-align: right'>"
            + "<strong>{{epacts:MARKER_ID}}</strong><br>"
            + "Chrom: <strong>{{epacts:CHROM}}</strong><br/>"
            + "Pos: <strong>{{epacts:BEGIN}}</strong><br/>"
            + "P Value: <strong>{{epacts:PVALUE|scinotation2}}</strong><br>"
            + ((has_maf)? "MAF: <strong>{{epacts:MAF|sig3}}</strong><br/>" : "")
            + ((has_beta) ? "BETA: <strong>{{epacts:BETA|sig3}}</strong><br/>" : "")
            + ((has_ns) ? "N: <strong>{{epacts:NS}}</strong><br/>" : "")
            + "</div>";
        if (has_maf) {
            fields.push("epacts:MAF");
        }
        if (has_beta) {
            fields.push("epacts:BETA");
        }
        if (has_ns) {
            fields.push("epacts:NS");
        }
        if (has_ld) {
            fields.push("ld:state");
            fields.push("ld:isrefvar");
        }
        var assoc_mods = {
            dashboard: {components: []},
            data_layers: [
                LocusZoom.Layouts.get("data_layer", "significance")
            ]
        };
        if ( build != "GRCh38" ) {
            assoc_mods.data_layers.push( LocusZoom.Layouts.get("data_layer", "recomb_rate") );
        }
        let tooltip = LocusZoom.Layouts.get("tooltip", "standard_association");
        tooltip.html = tooltipText;
        tooltip.closable = false;
        assoc_mods.data_layers.push( 
            LocusZoom.Layouts.get("data_layer", "association_pvalues", {
                namespace: {"assoc": "epacts"} ,
                fields: fields,
                id_field: fields[0],
                x_axis: {field: "epacts:BEGIN"},
                y_axis: {field: "epacts:PVALUE|neglog10"},
                tooltip: tooltip,
                behaviors: {"onclick": [{
                    action: "link",
                    status: "selected",
                    href: "../variant?chrom={{epacts:CHROM}}&pos={{epacts:BEGIN}}&variant_id={{epacts:MARKER_ID|urlencode}}"
                }]}
            })
        );
        var gene_data_layer = LocusZoom.Layouts.get("data_layer", "genes");
        tooltip.closable = false;
        var gene_tooltip = LocusZoom.Layouts.get("tooltip", "standard_genes");
        gene_tooltip.html = "<h4><strong><i>{{gene_name}}</i></strong></h4>" +
            "<p>Gene ID: <strong>{{gene_id}}</strong></p>" +
            "<p>Transcript ID: <strong>{{transcript_id}}</strong></p>";
        gene_data_layer.fields =  ["gene:gene"];
        gene_data_layer.tooltip = gene_tooltip;

        var gene_mods = {
            namespace: {"gene": "gene"},
            dashboard: {components: []},
            data_layers: [ gene_data_layer ],
        };
        var layout = {
            width: 1000,
            height: 500,
            responsive_resize: true,
            panels : [
                LocusZoom.Layouts.get("panel", "association", assoc_mods),
                LocusZoom.Layouts.get("panel", "genes", gene_mods)
            ]
        };
        if (variant) {
            layout.state = {ldrefvar: variant};
        }
        return layout;
    }

    function move(plot, direction) {
        // 1 means right, -1 means left.
        var start = plot.state.start;
        var end = plot.state.end;
        var shift = Math.floor((end - start) / 2) * direction;
        plot.applyState({
            chr: plot.state.chr,
            start: start + shift,
            end: end + shift
        });
    }

    function zoom(plot, growth_factor){
        // 2 means bigger view, 0.5 means zoom in.
        growth_factor = parseFloat(growth_factor);
        var delta = (plot.state.end - plot.state.start) * (growth_factor - 1) / 2;
        var new_start = Math.max(Math.round(plot.state.start - delta), 1);
        var new_end   = Math.round(plot.state.end + delta);
        if (new_start == new_end){ new_end++; }
        var new_state = {
            start: new_start,
            end: new_end
        };
        if (new_state.end - new_state.start > plot.layout.max_region_scale){
            delta = Math.round(((new_state.end - new_state.start) - plot.layout.max_region_scale) / 2);
            new_state.start += delta;
            new_state.end -= delta;
        }
        if (new_state.end - new_state.start < plot.layout.min_region_scale){
            delta = Math.round((plot.layout.min_region_scale - (new_state.end - new_state.start)) / 2);
            new_state.start -= delta;
            new_state.end += delta;
        }
        plot.applyState(new_state);
    }

    fetch(api_url).then(x => x.json()).then(function(x) {
        var cols = [];
        if (x.header && x.header.variant_columns) {
            cols = x.header.variant_columns;
        }
        var layout = getlayout(cols, genome_build);
        lzplot = LocusZoom.populate("#locuszoom", data_sources, layout);

        $(".control-buttons .pan-left").on("click", function() {move(lzplot, -0.5);});
        $(".control-buttons .pan-right").on("click", function() {move(lzplot, 0.5);});
        $(".control-buttons .zoom-in").on("click", function() {zoom(lzplot, 1/1.5);});
        $(".control-buttons .zoom-out").on("click", function() {zoom(lzplot, 1.5);});
    });

});


