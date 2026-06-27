#include <QtTest>

#include "infrastructure/network/VisionApiClient.cpp"

namespace {
QByteArray chatResponse(const QJsonValue &content)
{
    const QJsonObject root{
        {QStringLiteral("choices"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("message"),
                  QJsonObject{
                      {QStringLiteral("content"), content}
                  }}
             }
         }}
    };
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
}

class VisionApiClientJsonTest : public QObject {
    Q_OBJECT

private slots:
    void parseAssistantJson_acceptsPureJson()
    {
        QString error;
        const auto payload = parseAssistantJson(chatResponse(QStringLiteral("{\"status\":\"ok\"}")), &error);

        QVERIFY2(payload.has_value(), qPrintable(error));
        QCOMPARE(payload->value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    }

    void parseAssistantJson_acceptsMarkdownJson()
    {
        QString error;
        const auto payload = parseAssistantJson(
            chatResponse(QStringLiteral("```json\n{\"caption\":\"frame\",\"tags\":[\"a\"]}\n```")),
            &error);

        QVERIFY2(payload.has_value(), qPrintable(error));
        QCOMPARE(payload->value(QStringLiteral("caption")).toString(), QStringLiteral("frame"));
    }

    void parseAssistantJson_acceptsJsonWithTextAroundIt()
    {
        QString error;
        const auto payload = parseAssistantJson(
            chatResponse(QStringLiteral("Result:\n{\"caption\":\"frame\",\"objects\":[\"camera\"]}\nDone.")),
            &error);

        QVERIFY2(payload.has_value(), qPrintable(error));
        QCOMPARE(payload->value(QStringLiteral("caption")).toString(), QStringLiteral("frame"));
    }

    void parseAssistantJson_prefersLastValidObject()
    {
        QString error;
        const auto payload = parseAssistantJson(
            chatResponse(QStringLiteral("Schema: {\"caption\":\"\"}\nActual: {\"caption\":\"real frame\"}")),
            &error);

        QVERIFY2(payload.has_value(), qPrintable(error));
        QCOMPARE(payload->value(QStringLiteral("caption")).toString(), QStringLiteral("real frame"));
    }

    void parseAssistantJson_acceptsJsonAfterReasoningTag()
    {
        QString error;
        const auto payload = parseAssistantJson(
            chatResponse(QStringLiteral("<think>reasoning with {braces}</think>\n{\"summary\":\"video\"}")),
            &error);

        QVERIFY2(payload.has_value(), qPrintable(error));
        QCOMPARE(payload->value(QStringLiteral("summary")).toString(), QStringLiteral("video"));
    }

    void parseAssistantJson_rejectsContentWithoutJsonObject()
    {
        QString error;
        const auto payload = parseAssistantJson(chatResponse(QStringLiteral("no json here")), &error);

        QVERIFY(!payload.has_value());
        QVERIFY(!error.isEmpty());
    }

    void parseAssistantJson_acceptsTextContentArray()
    {
        const QJsonArray content{
            QJsonObject{
                {QStringLiteral("type"), QStringLiteral("text")},
                {QStringLiteral("text"), QStringLiteral("Here: {\"status\":\"ok\"}")}
            }
        };

        QString error;
        const auto payload = parseAssistantJson(chatResponse(content), &error);

        QVERIFY2(payload.has_value(), qPrintable(error));
        QCOMPARE(payload->value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    }
};

QTEST_MAIN(VisionApiClientJsonTest)

#include "VisionApiClientJsonTest.moc"
