
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


    $.getJSON("/api/jobs/" + job_id + "/plots/manhattan").done(function(variants)
    {
        create_gwas_plot("#tab1", variants.variant_bins, variants.unbinned_variants, function(chrom, pos, ref, alt)
        {
            console.log(chrom, pos, ref, alt);
			jumpToLocusZoom(chrom, pos);
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
	$.getJSON("/api/jobs/" + job_id + "/tables/top").done(function(data) {
		data = data.data || data
		chrpos = {};
		var i=0;
		for(i=1; i<=22; i++) {
			chrpos[i.toString()] = i
		}
		chrpos["X"] = i++;
		chrpos["Y"] = i++;
		chrpos["XY"] = i++;
		chrpos["MT"] = i++;
		for(var j=0; j<data.length; j++) {
			chr = data[j].chrom.replace("chr","")
			if (chrpos[chr] !== undefined) {
				data[j].chrom_sort = chrpos[chr]
			} else {
				data[j].chrom_sort = chrpos[chr] = i++
			}
		}
		var table = $("#tophits").DataTable( {
			data: data,
			columns: [
				{data: null, title:"Chrom",
					orderData: [0,1],
					type: "num",
					render: function(data, type) {
						if (type=="sort") {
							return data.chrom_sort
						}
						return data.chrom
					},
					className: "dt-body-right"
				},
				{data: "pos", title:"Position", 
					render: function(data, type) {
						if (type=="display") {
							return data.toLocaleString()
						}
						return data
					},
					orderData: [0,1],
					className: "dt-body-right"
				},
				{data: "name", title:"Variant"},
				{data: "pval", title:"Most Significant P-Value", 
					render: function(data, type) {
						if (type=="display") {
							return data.toExponential(2)
						}
						return data
					},
					className: "dt-body-right"
				},
				{data: "sig_count", title:"# Significant Hits",
					className: "dt-body-center"
				},
				{data: "gene", title:"Nearest gene",
					className: "dt-body-center"
				},
				{data: "pos", title:"Plot",
					render:function(data, type, row) {
						var fn = "event.preventDefault();" + 
							"jumpToLocusZoom(\"" + row.chrom + "\"," + data + ")";
						return "<a href='#' onclick='" + fn + "'>View</a>"
					},
					orderable: false,
					className: "dt-body-center"
				}
			],
			order: [[3, "asc"]],
			lengthChange: false,
			searching: false,
			dom: 'rtip',
		})
		//$("#tophits").on("click","tr",function(event) {
		//	var data = table.row(this).data()
		//	jumpToLocusZoom(data.chrom, data.peak);
		//})
	}).fail(function() {
		$("ul.tabs li[rel='tab3'").remove()
	});
});

jumpToLocusZoom = function(chr, pos) {
	if (job_id && chr && pos) {
		var region = chr + ":" + (pos-100000) + "-" + (pos+100000);
		document.location.href = "/jobs/" + job_id + "/locuszoom/" + region;
	}
}
