import QtQuick 2.3
import QtQuick.Layouts 1.2
import QtQuick.Controls 1.4
import QtGraphicalEffects 1.0
import radiance 1.0

ApplicationWindow {
    id: window;
    visible: true;

    Model {
        id: model;
        onVideoNodeAdded: {
            console.log("Added video node", videoNode);
        }
        onVideoNodeRemoved: {
            console.log("Removed video node", videoNode);
        }
        onEdgeAdded: {
            console.log("Added edge");
        }
        onEdgeRemoved: {
            console.log("Removed edge");
        }
        onGraphChanged: {
            console.log("Graph Changed");
        }
    }

    EffectNode {
        id: en
        name: "yellow"
        intensity: ent.intensity
    }
    EffectNode {
        id: en2
        name: "heart"
        intensity: ent2.intensity
    }
    EffectNode {
        id: en3
        name: "wwave"
        intensity: ent3.intensity
    }
    EffectNode {
        id: en4
        name: "wwave"
        intensity: ent4.intensity
    }

    Component.onCompleted: {
        UISettings.previewSize = "100x100";
        UISettings.outputSize = "1024x768";
        model.addVideoNode(en);
        model.addVideoNode(en2);
        model.addVideoNode(en3);
        model.addVideoNode(en4);
        model.addEdge(en, en2, 0);
        model.addEdge(en2, en3, 0);
        model.addEdge(en3, en4, 0);
        RenderContext.addRenderTrigger(window, model, 0);
        console.log(model.graph.vertices[0]), 
        console.log(model.graph.vertices[1]), 
        console.log(model.graph.edges[0].fromVertex, 
                    model.graph.edges[0].toVertex, 
                    model.graph.edges[0].toInput);
    }

    ColumnLayout {
        Rectangle {
            color: "#FF0000"
            width: 500
            height: 500
            VideoNodeRender {
                id: vnr
                anchors.fill: parent
                chain: 0
                videoNode: en2
            }
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    vnr.update()
                    model.removeVideoNode(en4);
                }
            }
        }

        RowLayout {
            EffectNodeTile {
                id: ent;
                effect: en;
            }
            EffectNodeTile {
                id: ent2;
                effect: en2;
            }
            EffectNodeTile {
                id: ent3;
                effect: en3;
            }
            EffectNodeTile {
                id: ent4;
                effect: en4;
            }
        }
    }

    Action {
        id: quitAction
        text: "&Quit"
        onTriggered: Qt.quit()
    }
}
