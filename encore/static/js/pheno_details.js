/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "vars": "local" }] */

function init_editform(pheno_id, pheno_api_url) {
    var edit_icon = $("<span>", {class: "glyphicon glyphicon-pencil", "aria-hidden": "true"});
    var $dialog = $("#editModal");
    var $title = $("#pheno_main_title");
    $title.append($("<sup>").append($("<span>", {class: "label label-edit"}).append($("<a>", { class:"edit-pheno-modal"}).append(edit_icon))));
    $dialog.find("form").on("keyup keypress", function(e) {
        var keyCode = e.keyCode || e.which;
        if (keyCode === 13) { 
            e.preventDefault();
            return false;
        }
    });
    $("a.edit-pheno-modal").click(function(evt) {
        evt.preventDefault();
        $.getJSON(pheno_api_url).then(function(resp) {
            $dialog.find("#pheno_name").val(resp.name);
            $dialog.on("shown.bs.modal", function() {
                $dialog.find("#pheno_name").focus();
            });
            $dialog.modal();
        });
    });
    $("button.edit-pheno-save").click(function(evt) {
        evt.preventDefault();
        var new_name = $dialog.find("#pheno_name").val();
        $.post(pheno_api_url, {"name": new_name}).done( function() {
            $title.contents().first().replaceWith(new_name);
            $dialog.modal("hide");
        }).fail(function() {
            alert("Update failed");
        });
    });
}

function init_new_job_button(selector, pheno_error) {
    selector = selector || "button[name=new_job]";
    if (!pheno_error) {
        $(selector).click(function(evt) {
            evt.preventDefault();
            var url = $(evt.target).data("link");
            document.location = url;
        });
    } else {
        $(selector).prop("title", pheno_error)
            .prop("disabled", true);
    }
}

function init_pheno_delete_button(selector) {
    selector = selector || "button[name=delete_pheno]";
    $(selector).click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("action");
        $("#deleteModal button.delete-pheno").data("action", url);
        $("#deleteModal").modal();
    });
    $("#deleteModal button.delete-pheno").click(function(evt) {
        evt.preventDefault();
        var url = $(evt.target).data("action");
        $.ajax({
            url: url, 
            type: "DELETE",
            success: function() {
                document.location.reload();
            },
            error: function() {
                alert("Deletion request failed");
            }
        });
    });
}
