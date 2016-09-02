
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
        xhr.addEventListener("load", function(ev)
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
        create_gwas_plot(selector, variants.variant_bins, variants.unbinned_variants, function(chrom, pos, ref, alt)
        {
            console.log(chrom, pos, ref, alt);
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
        create_qq_plot("#tab2", data);
    });
}

function init_tophits(job_id, selector, data_url) {
	selector = selector || "#tophits";
	data_url = data_url|| "/api/jobs/" + job_id + "/tables/top"
	$.getJSON(data_url).done(function(data) {
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
		var table = $(selector).DataTable( {
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
							"jumpToLocusZoom(\"" + job_id + "\",\"" + row.chrom + "\"," + data + ")";
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
};


function init_chunk_progress(job_id, selector) {
	selector = selector || "#progress";
    $.getJSON("/api/jobs/" + job_id + "/chunks").done(function(chunks) {
        if (chunks.length<1) {
            return;
        }
        var ideo = new Ideogram(selector);
        $(selector).append("<h3>Progress</h3>")
        chunks = chunks.map(function(x) {
            x.chrom = "chr" + x.chr;
            x.fill = "#3CA661";
            return x;
        });
        ideo.setRegions(chunks);
        ideo.draw();
    });
};

jumpToLocusZoom = function(job_id, chr, pos) {
	if (job_id && chr && pos) {
		var region = chr + ":" + (pos-100000) + "-" + (pos+100000);
		document.location.href = "/jobs/" + job_id + "/locuszoom/" + region;
	}
}


