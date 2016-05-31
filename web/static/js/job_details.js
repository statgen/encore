
$(document).ready(function()
{
    $(".tab-content").hide();
    $(".tab-content:first").show();

    $("ul.tabs li").click(function()
    {
        $("ul.tabs li").removeClass("active");
        $(this).addClass("active");
        $(".tab-content").hide();
        var activeTab = $(this).attr("rel");
        $("#"+activeTab).fadeIn();
    });

    $("#back_arrow").click(function()
    {
        window.location = history.go(-1);
    });

    $("button[name=cancel_job]").click(function()
    {
        var res = /^\/jobs\/(.*)$/.exec(window.location.pathname);
        if (res.length === 2)
        {
            var job_id = res[1];
            var xhr = new XMLHttpRequest();
            xhr.addEventListener("load", function(ev)
            {
               location.reload();
            }, false);

            xhr.addEventListener("error", function() { alert("Request Failed"); }, false);
            xhr.open("POST", "/api/jobs/" + job_id + "/cancel_request");
            xhr.send();
        }
    });
});