
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
});