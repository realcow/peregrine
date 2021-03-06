import QtQuick 2.0

Item {
    id: customUiRoot;
    objectName: "customUiRoot"

    function loadCustomUi(qmlObjStr, argsForActivatedEvent) {
        try {
            var created = Qt.createQmlObject(qmlObjStr, customUiRoot);
            created.onActivated(argsForActivatedEvent);
        } catch(err) {
        }
    }

    function clearChildren() {
        for (var i = 0; i < children.length; i++) {
            children[i].destroy();
        }
    }
}
