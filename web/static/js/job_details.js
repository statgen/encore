
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
});

jumpToLocusZoom = function(chr, pos) {
	if (job_id && chr && pos) {
		var region = chr + ":" + (pos-100000) + "-" + (pos+100000);
		document.location.href = "/jobs/" + job_id + "/locuszoom/" + region;
	}
}
