/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "vars": "local" }] */

function init_vcf_stats(url) {
    $.getJSON(url).then(function(resp) {
        var stats = resp[0];
        $("#carousel h3.name").text(stats.name);
        var fields = ["genotype_count", "record_count", "sample_count"];
        fields.forEach(function(x) {
            var sel = "#carousel tr." + x + " td:nth-child(1)";
            var $ele = $(sel);
            var val = stats[x] && parseInt(stats[x]).toLocaleString();
            if ($ele && val) {
                $ele.text(val);
            }

        });
    });
}


