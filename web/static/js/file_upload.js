/* eslint-env jquery */
/* eslint no-unused-vars: ["error", { "varsIgnorePattern": "UploadManager" }] */

function UploadManager(selector) {
    var dropped_files = [];
    var ubox = $(selector);
    var fileInput = ubox.find("input:file").first();
    var submitButton = ubox.find(".ubox-button").first();
    this.onupload = null;

    function setProgressBarValue(progress) {
        ubox.find(".ubox-progress").css("width", progress.toString() + "%");
    }

    function uploadProgress(evt) {
        if (evt.lengthComputable) {
            var percentComplete = Math.round(evt.loaded * 100 / evt.total);
            setProgressBarValue(percentComplete);
        } else {
            //document.getElementById("progressNumber").innerHTML = "unable to compute";
        }
    }

    function uploadFailed(msg) {
        setProgressBarValue(0);
        $("body").removeClass("wait");
        alert("There was an error attempting to upload the file" + ((msg)?": "+msg:""));
    }

    function uploadCanceled() {
        setProgressBarValue(0);
        $("body").removeClass("wait");
        alert("The upload has been canceled by the user or the browser dropped the connection.");
    }

    var uploadComplete = function(resp) {
        setProgressBarValue(0);
        $("body").removeClass("wait");
        if (this.onupload) {
            this.onupload(resp);
        }
    }.bind(this);

    var uploadFile = function() {
        $("body").addClass("wait");
        submitButton.prop("disabled", true);
        var fd = new FormData();
        var fieldName = fileInput.attr("name");
        fd.append(fieldName, dropped_files[0]);
        fd.append(fieldName + "_name", dropped_files[0].name);
        var xhr = new XMLHttpRequest();
        xhr.upload.addEventListener("progress", uploadProgress, false);
        xhr.addEventListener("load", function () {
            /* This event is raised when the server send back a response */
            if (xhr.status >= 200 && xhr.status < 300) {
                try {
                    var resp = JSON.parse(xhr.responseText);
                    uploadComplete(resp);
                } catch (ex) {
                    uploadComplete(xhr.responseText);
                }
            } else {
                try {
                    var resp = JSON.parse(xhr.responseText);
                    uploadFailed(resp.error);
                } catch (ex) {
                    uploadFailed(xhr.statusText);
                }
            }
            dropped_files = [];
            checkReadyToUpload();
            submitButton.prop("disabled", false);
        }.bind(this), false);
        xhr.addEventListener("error", uploadFailed, false);
        xhr.addEventListener("abort", uploadCanceled, false);
        xhr.open("POST", ubox.attr("action"));
        xhr.send(fd);
    }.bind(this);

    var checkReadyToUpload = function () {
        if (dropped_files.length) {
            submitButton.show().text("Upload File (" + dropped_files[0].name + ")");
        } else {
            submitButton.hide();
        }
    };


    var attachEvents = function() {
        ubox.on("drag dragstart dragend dragover dragenter dragleave drop", function(e) {
            e.preventDefault();
            e.stopPropagation();
        })
        .on("dragover dragenter", function() {
            ubox.addClass("is-dragged-over");
        })
        .on("dragleave dragend drop", function() {
            ubox.removeClass("is-dragged-over");
        })
        .on("drop", function(e) {
            if (e.originalEvent.dataTransfer.items.length) {// TODO: Filter directory.
                dropped_files = e.originalEvent.dataTransfer.files;
            } else {
                dropped_files = [];
            }
            checkReadyToUpload();
        });
        fileInput.on("change", function() {
            dropped_files = fileInput[0].files || [];
            checkReadyToUpload();
        });
        submitButton.on("click", function() {
            uploadFile();
        });
    };

    var has_modern_upload = function() {
        var div = document.createElement("div");
        return (("draggable" in div) || ("ondragstart" in div && "ondrop" in div)) && "FormData" in window && "FileReader" in window;
    }();

    if (!has_modern_upload) {
        alert("Browser not supported");
    } else {
        attachEvents();
    }
}
