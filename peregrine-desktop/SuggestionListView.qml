import QtQuick 2.0

ListView {
    id: suggestionListView
    objectName: "suggestionListView"
    width: 320;
    height: 200;

    currentIndex: -1

    model: ListModel {
        id: suggestionModel
        objectName: "suggestionModel"
    }

    delegate: Item {
        width: parent.width
        height: 40
        Image {
            x: 0; y: 0
            width: 40; height: 40
            source: image
        }
        Text {
            x: 40; y: 0
            text: value
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                currentIndex = index;
                inputHandlerDelegate.onSuggestionItemClicked(index);
                currentIndex = -1;
            }
        }
    }

    highlight: Rectangle {
        color: "lightsteelblue"
    }

    function moveSelectUp() {
        if (count == 0) { return; }
        if (currentIndex == -1) {
            currentIndex = count - 1;
            return;
        }
        currentIndex = currentIndex - 1;
    }

    function moveSelectDown() {
        if (count == 0) { return; }
        if (currentIndex == -1) {
            currentIndex = 0;
            return;
        }
        currentIndex = currentIndex + 1
        if (count == currentIndex) {
            currentIndex = 0;
        }
    }

    function runSelected() {
        if (currentIndex == -1) {
            return;
        }
        inputHandlerDelegate.onSuggestionItemClicked(currentIndex);
        currentIndex = -1;
    }

    function addItem(val, image) {
        model.append({ value: val, image: image });
    }

    function clearItems() {
        model.clear();
        currentIndex = -1;
    }

    function fitHeightToChildren() {
        height = childrenRect.height;
    }
}
