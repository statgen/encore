/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "vars": "local" }] */

function init_vcf_stats(url) {
    var $info = $("#carousel.vcf-stats");
    if ($info.length) {
        $.getJSON(url).then(function(resp) {
            if (resp.header) resp=resp.data;
            var stats = resp[0];
            $("h3.name", $info).text(stats.name);
            var fields = ["genotype_count", "record_count", "sample_count"];
            fields.forEach(function(x) {
                var sel = "tr." + x + " td:nth-child(1)";
                var $ele = $(sel, $info);
                var val = stats[x] && parseInt(stats[x]).toLocaleString();
                if ($ele && val) {
                    $ele.text(val);
                }

            });
        });
    }
}


